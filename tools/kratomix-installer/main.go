package main

import (
	"archive/zip"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"image/color"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/canvas"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/dialog"
	"fyne.io/fyne/v2/theme"
	"fyne.io/fyne/v2/widget"
)

const defaultManifestURL = "https://github.com/kratofl/kratomix/releases/latest/download/manifest.json"

var httpClient = &http.Client{Timeout: 12 * time.Second}

type manifest struct {
	Schema     int      `json:"schema"`
	Brand      string   `json:"brand"`
	Version    string   `json:"version"`
	ReleaseTag string   `json:"releaseTag"`
	Plugins    []plugin `json:"plugins"`
}

type plugin struct {
	Slug    string          `json:"slug"`
	Name    string          `json:"name"`
	Version string          `json:"version"`
	Asset   string          `json:"asset"`
	URL     string          `json:"url"`
	Formats map[string]bool `json:"formats"`
}

type pluginRow struct {
	plugin plugin
	check  *widget.Check
}

type cliOptions struct {
	headless bool
	list     bool
	manifest string
	plugins  string
	formats  string
}

func parseCLIOptions() cliOptions {
	options := cliOptions{manifest: defaultManifestURL, plugins: "all", formats: "au,vst3"}
	flag.BoolVar(&options.headless, "headless", false, "run without the GUI")
	flag.BoolVar(&options.list, "list", false, "list available plugins and exit")
	flag.StringVar(&options.manifest, "manifest", defaultManifestURL, "release manifest URL or local path")
	flag.StringVar(&options.plugins, "plugins", "all", "comma-separated plugin slugs or all")
	flag.StringVar(&options.formats, "formats", "au,vst3", "comma-separated formats: au,vst3")
	flag.Parse()
	return options
}

func runHeadless(options cliOptions) error {
	m, source, err := loadManifestWithFallback(options.manifest, true)
	if err != nil {
		return err
	}

	selected, err := filterPlugins(m.Plugins, options.plugins)
	if err != nil {
		return err
	}

	if options.list {
		fmt.Printf("%s %s from %s\n", m.Brand, m.Version, source)
		for _, p := range selected {
			fmt.Printf("%s\t%s\t%s\n", p.Slug, p.Version, availableFormatText(p))
		}
		return nil
	}

	formats := parseCSV(options.formats)
	if len(formats) == 0 {
		return errors.New("select at least one format")
	}

	tempDir, err := os.MkdirTemp("", "kratomix-installer-*")
	if err != nil {
		return err
	}
	defer os.RemoveAll(tempDir)

	logf := func(format string, args ...any) {
		fmt.Printf(format+"\n", args...)
	}

	for _, p := range selected {
		if err := installPlugin(tempDir, p, formats, logf); err != nil {
			return err
		}
	}

	fmt.Println("Install complete. Restart your DAW and rescan plugins if needed.")
	return nil
}

func filterPlugins(plugins []plugin, selection string) ([]plugin, error) {
	if strings.TrimSpace(selection) == "" || selection == "all" {
		return plugins, nil
	}

	wanted := make(map[string]bool)
	for _, slug := range parseCSV(selection) {
		wanted[slug] = true
	}

	selected := make([]plugin, 0, len(wanted))
	for _, p := range plugins {
		if wanted[p.Slug] {
			selected = append(selected, p)
			delete(wanted, p.Slug)
		}
	}

	if len(wanted) > 0 {
		missing := make([]string, 0, len(wanted))
		for slug := range wanted {
			missing = append(missing, slug)
		}
		return nil, fmt.Errorf("unknown plugin slug: %s", strings.Join(missing, ", "))
	}

	return selected, nil
}

func parseCSV(value string) []string {
	parts := strings.Split(value, ",")
	result := make([]string, 0, len(parts))
	for _, part := range parts {
		trimmed := strings.TrimSpace(part)
		if trimmed != "" {
			result = append(result, trimmed)
		}
	}
	return result
}

func main() {
	options := parseCLIOptions()
	if options.headless {
		if err := runHeadless(options); err != nil {
			fmt.Fprintln(os.Stderr, err)
			os.Exit(1)
		}
		return
	}

	a := app.NewWithID("com.kratomix.installer")
	a.Settings().SetTheme(kratomixTheme{})
	w := a.NewWindow("Kratomix Installer")
	w.Resize(fyne.NewSize(820, 560))
	if icon := loadIconResource(); icon != nil {
		w.SetIcon(icon)
	}

	manifestEntry := widget.NewEntry()
	manifestEntry.SetText(options.manifest)

	pluginRows := container.NewVBox()
	logText := widget.NewMultiLineEntry()
	logText.Disable()
	statusLabel := widget.NewLabel("Loading release manifest...")

	auCheck := widget.NewCheck("AU", nil)
	auCheck.SetChecked(true)
	vst3Check := widget.NewCheck("VST3", nil)
	vst3Check.SetChecked(true)

	var rows []pluginRow
	var current manifest

	logf := func(format string, args ...any) {
		line := fmt.Sprintf(format, args...)
		logText.SetText(strings.TrimSpace(logText.Text + "\n" + line))
		logText.CursorRow = len(strings.Split(logText.Text, "\n"))
	}

	applyManifest := func(m manifest, source string) {
		current = m
		rows = rows[:0]
		pluginRows.RemoveAll()

		for _, p := range current.Plugins {
			p := p
			label := fmt.Sprintf("%s  %s", p.Name, p.Version)
			check := widget.NewCheck(label, nil)
			check.SetChecked(true)
			rows = append(rows, pluginRow{plugin: p, check: check})
			formatText := availableFormatText(p)
			pluginRows.Add(container.NewBorder(nil, nil, check, widget.NewLabel(formatText)))
		}

		pluginRows.Refresh()
		statusLabel.SetText(fmt.Sprintf("%s %s loaded", current.Brand, current.Version))
		logf("Loaded %d plugins from %s", len(current.Plugins), source)
	}

	loadFromSource := func(source string, useFallback bool) {
		logf("Loading manifest: %s", source)
		m, loadedFrom, err := loadManifestWithFallback(source, useFallback)
		if err != nil {
			dialog.ShowError(err, w)
			statusLabel.SetText("No release manifest loaded")
			logf("Manifest load failed: %v", err)
			return
		}

		applyManifest(m, loadedFrom)
	}

	loadButton := widget.NewButton("Load", func() {
		url := strings.TrimSpace(manifestEntry.Text)
		if url == "" {
			dialog.ShowError(errors.New("manifest URL is required"), w)
			return
		}

		loadFromSource(url, true)
	})

	selectAllButton := widget.NewButton("Select All", func() {
		for _, row := range rows {
			row.check.SetChecked(true)
		}
	})

	clearButton := widget.NewButton("Clear", func() {
		for _, row := range rows {
			row.check.SetChecked(false)
		}
	})

	var installButton *widget.Button
	installButton = widget.NewButton("Install Selected", func() {
		if len(rows) == 0 {
			dialog.ShowError(errors.New("load a manifest before installing"), w)
			return
		}

		formats := selectedFormats(auCheck.Checked, vst3Check.Checked)
		if len(formats) == 0 {
			dialog.ShowError(errors.New("select at least one format"), w)
			return
		}

		selected := make([]plugin, 0)
		for _, row := range rows {
			if row.check.Checked {
				selected = append(selected, row.plugin)
			}
		}
		if len(selected) == 0 {
			dialog.ShowError(errors.New("select at least one plugin"), w)
			return
		}

		installButton.Disable()
		defer installButton.Enable()

		tempDir, err := os.MkdirTemp("", "kratomix-installer-*")
		if err != nil {
			dialog.ShowError(err, w)
			return
		}
		defer os.RemoveAll(tempDir)

		for _, p := range selected {
			logf("Installing %s", p.Name)
			if err := installPlugin(tempDir, p, formats, logf); err != nil {
				dialog.ShowError(err, w)
				logf("Install failed: %v", err)
				return
			}
		}

		logf("Install complete. Restart your DAW and rescan plugins if needed.")
		dialog.ShowInformation("Kratomix Installer", "Install complete. Restart your DAW and rescan plugins if needed.", w)
	})

	title := canvas.NewText("Kratomix", color.NRGBA{R: 246, G: 226, B: 192, A: 255})
	title.TextStyle = fyne.TextStyle{Bold: true, Italic: true}
	title.TextSize = 34
	subtitle := canvas.NewText("Installer", color.NRGBA{R: 241, G: 194, B: 124, A: 255})
	subtitle.TextStyle = fyne.TextStyle{Bold: true}
	subtitle.TextSize = 18

	formatBox := container.NewHBox(widget.NewLabel("Formats"), auCheck, vst3Check)
	selectionBox := container.NewHBox(selectAllButton, clearButton)
	installTab := container.NewBorder(
		container.NewVBox(container.NewHBox(title, subtitle), statusLabel, formatBox, selectionBox),
		container.NewVBox(logText, installButton),
		nil,
		nil,
		container.NewVScroll(pluginRows),
	)

	settingsTab := container.NewBorder(
		container.NewVBox(widget.NewLabel("Release Manifest"), container.NewBorder(nil, nil, nil, loadButton, manifestEntry)),
		nil,
		nil,
		nil,
		widget.NewLabel("The installer loads GitHub releases by default and falls back to the bundled manifest when offline."),
	)

	tabs := container.NewAppTabs(
		container.NewTabItem("Install", installTab),
		container.NewTabItem("Settings", settingsTab),
	)
	w.SetContent(tabs)
	go func() {
		m, source, err := loadManifestWithFallback(options.manifest, true)
		fyne.Do(func() {
			if err != nil {
				statusLabel.SetText("No release manifest loaded")
				logf("Manifest load failed: %v", err)
				return
			}
			applyManifest(m, source)
		})
	}()
	w.ShowAndRun()
}

func selectedFormats(au bool, vst3 bool) []string {
	formats := make([]string, 0, 2)
	if au {
		formats = append(formats, "au")
	}
	if vst3 {
		formats = append(formats, "vst3")
	}
	return formats
}

func availableFormatText(p plugin) string {
	formats := make([]string, 0, 2)
	if p.Formats["au"] {
		formats = append(formats, "AU")
	}
	if p.Formats["vst3"] {
		formats = append(formats, "VST3")
	}
	if len(formats) == 0 {
		return "No formats"
	}
	return strings.Join(formats, " + ")
}

func loadManifest(source string) (manifest, error) {
	var body []byte
	var err error

	if strings.HasPrefix(source, "http://") || strings.HasPrefix(source, "https://") {
		resp, err := httpClient.Get(source)
		if err != nil {
			return manifest{}, err
		}
		defer resp.Body.Close()
		if resp.StatusCode < 200 || resp.StatusCode >= 300 {
			return manifest{}, fmt.Errorf("manifest request failed: %s", resp.Status)
		}
		body, err = io.ReadAll(resp.Body)
	} else {
		body, err = os.ReadFile(source)
	}
	if err != nil {
		return manifest{}, err
	}

	var m manifest
	if err := json.Unmarshal(body, &m); err != nil {
		return manifest{}, err
	}
	if len(m.Plugins) == 0 {
		return manifest{}, errors.New("manifest contains no plugins")
	}
	return m, nil
}

func loadManifestWithFallback(primary string, useFallback bool) (manifest, string, error) {
	m, err := loadManifest(primary)
	if err == nil {
		return m, primary, nil
	}

	if !useFallback {
		return manifest{}, "", err
	}

	var failures []string
	failures = append(failures, fmt.Sprintf("%s: %v", primary, err))

	for _, candidate := range localManifestCandidates() {
		m, err := loadManifest(candidate)
		if err == nil {
			return m, candidate, nil
		}
		failures = append(failures, fmt.Sprintf("%s: %v", candidate, err))
	}

	return manifest{}, "", fmt.Errorf("could not load manifest\n%s", strings.Join(failures, "\n"))
}

func localManifestCandidates() []string {
	candidates := []string{
		"manifest.json",
		filepath.Join("dist", "manifest.json"),
		filepath.Join("..", "..", "dist", "manifest.json"),
	}

	if executable, err := os.Executable(); err == nil {
		resourcesManifest := filepath.Clean(filepath.Join(filepath.Dir(executable), "..", "Resources", "manifest.json"))
		candidates = append([]string{resourcesManifest}, candidates...)
	}

	seen := make(map[string]bool)
	unique := make([]string, 0, len(candidates))
	for _, candidate := range candidates {
		if candidate == "" || seen[candidate] {
			continue
		}
		seen[candidate] = true
		unique = append(unique, candidate)
	}
	return unique
}

func installPlugin(tempDir string, p plugin, formats []string, logf func(string, ...any)) error {
	if p.URL == "" {
		return fmt.Errorf("%s has no download URL", p.Name)
	}

	zipPath := filepath.Join(tempDir, p.Asset)
	extractDir := filepath.Join(tempDir, p.Slug)

	logf("Downloading %s", p.URL)
	if err := downloadPluginAsset(zipPath, p); err != nil {
		return err
	}

	logf("Extracting %s", p.Asset)
	if err := unzip(zipPath, extractDir); err != nil {
		return err
	}

	for _, format := range formats {
		if !p.Formats[format] {
			logf("Skipping %s for %s; format is not available", strings.ToUpper(format), p.Name)
			continue
		}
		if err := installFormat(extractDir, p.Name, format, logf); err != nil {
			return err
		}
	}

	return nil
}

func downloadFile(path string, url string) error {
	resp, err := httpClient.Get(url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return fmt.Errorf("download failed: %s", resp.Status)
	}

	out, err := os.Create(path)
	if err != nil {
		return err
	}
	defer out.Close()

	_, err = io.Copy(out, resp.Body)
	return err
}

func downloadPluginAsset(path string, p plugin) error {
	if p.URL != "" {
		if err := downloadFile(path, p.URL); err == nil {
			return nil
		}
	}

	for _, candidate := range localAssetCandidates(p.Asset) {
		if err := copyFile(path, candidate); err == nil {
			return nil
		}
	}

	if p.URL == "" {
		return fmt.Errorf("%s has no download URL and no local asset named %s", p.Name, p.Asset)
	}
	return fmt.Errorf("could not download %s or find local asset %s", p.URL, p.Asset)
}

func localAssetCandidates(asset string) []string {
	candidates := []string{
		asset,
		filepath.Join("dist", asset),
		filepath.Join("..", "..", "dist", asset),
	}

	if executable, err := os.Executable(); err == nil {
		resourcesAsset := filepath.Clean(filepath.Join(filepath.Dir(executable), "..", "Resources", asset))
		candidates = append([]string{resourcesAsset}, candidates...)
	}

	return candidates
}

func copyFile(destination string, source string) error {
	in, err := os.Open(source)
	if err != nil {
		return err
	}
	defer in.Close()

	out, err := os.Create(destination)
	if err != nil {
		return err
	}
	defer out.Close()

	if _, err := io.Copy(out, in); err != nil {
		return err
	}
	return out.Close()
}

func unzip(zipPath string, destination string) error {
	reader, err := zip.OpenReader(zipPath)
	if err != nil {
		return err
	}
	defer reader.Close()

	for _, file := range reader.File {
		target := filepath.Join(destination, file.Name)
		cleanDestination, err := filepath.Abs(destination)
		if err != nil {
			return err
		}
		cleanTarget, err := filepath.Abs(target)
		if err != nil {
			return err
		}
		if !strings.HasPrefix(cleanTarget, cleanDestination+string(os.PathSeparator)) && cleanTarget != cleanDestination {
			return fmt.Errorf("refusing to extract unsafe path: %s", file.Name)
		}

		if file.FileInfo().IsDir() {
			if err := os.MkdirAll(target, file.Mode()); err != nil {
				return err
			}
			continue
		}

		if err := os.MkdirAll(filepath.Dir(target), 0o755); err != nil {
			return err
		}

		src, err := file.Open()
		if err != nil {
			return err
		}
		dst, err := os.OpenFile(target, os.O_WRONLY|os.O_CREATE|os.O_TRUNC, file.Mode())
		if err != nil {
			src.Close()
			return err
		}
		_, copyErr := io.Copy(dst, src)
		closeErr := dst.Close()
		src.Close()
		if copyErr != nil {
			return copyErr
		}
		if closeErr != nil {
			return closeErr
		}
	}

	return nil
}

func installFormat(extractDir string, productName string, format string, logf func(string, ...any)) error {
	var source string
	var target string

	switch format {
	case "au":
		source = filepath.Join(extractDir, "AU", productName+".component")
		target = filepath.Join("/Library/Audio/Plug-Ins/Components", productName+".component")
	case "vst3":
		source = filepath.Join(extractDir, "VST3", productName+".vst3")
		target = filepath.Join("/Library/Audio/Plug-Ins/VST3", productName+".vst3")
	default:
		return fmt.Errorf("unsupported format: %s", format)
	}

	if _, err := os.Stat(source); err != nil {
		return fmt.Errorf("missing %s bundle: %w", strings.ToUpper(format), err)
	}

	logf("Installing %s to %s", source, target)
	command := fmt.Sprintf(
		"mkdir -p %s && ditto %s %s && (xattr -dr com.apple.quarantine %s >/dev/null 2>&1 || true)",
		shellQuote(filepath.Dir(target)),
		shellQuote(source),
		shellQuote(target),
		shellQuote(target),
	)

	return runPrivileged(command)
}

func runPrivileged(command string) error {
	script := "do shell script " + strconv.Quote(command) + " with administrator privileges"
	output, err := exec.Command("osascript", "-e", script).CombinedOutput()
	if err != nil {
		return fmt.Errorf("administrator install failed: %w\n%s", err, strings.TrimSpace(string(output)))
	}
	return nil
}

func shellQuote(value string) string {
	return "'" + strings.ReplaceAll(value, "'", "'\\''") + "'"
}

func loadIconResource() fyne.Resource {
	for _, candidate := range iconCandidates() {
		resource, err := fyne.LoadResourceFromPath(candidate)
		if err == nil {
			return resource
		}
	}
	return nil
}

func iconCandidates() []string {
	candidates := []string{
		filepath.Join("assets", "icon.svg"),
		filepath.Join("tools", "kratomix-installer", "assets", "icon.svg"),
	}

	if executable, err := os.Executable(); err == nil {
		resourcesIcon := filepath.Clean(filepath.Join(filepath.Dir(executable), "..", "Resources", "icon.svg"))
		candidates = append([]string{resourcesIcon}, candidates...)
	}

	return candidates
}

type kratomixTheme struct{}

func (kratomixTheme) Color(name fyne.ThemeColorName, variant fyne.ThemeVariant) color.Color {
	switch name {
	case theme.ColorNameBackground:
		return color.NRGBA{R: 8, G: 9, B: 11, A: 255}
	case theme.ColorNameButton:
		return color.NRGBA{R: 37, G: 29, B: 23, A: 255}
	case theme.ColorNameDisabledButton:
		return color.NRGBA{R: 26, G: 24, B: 23, A: 255}
	case theme.ColorNameForeground:
		return color.NRGBA{R: 250, G: 231, B: 202, A: 255}
	case theme.ColorNameDisabled:
		return color.NRGBA{R: 126, G: 114, B: 96, A: 255}
	case theme.ColorNameForegroundOnPrimary:
		return color.NRGBA{R: 18, G: 14, B: 11, A: 255}
	case theme.ColorNameInputBackground:
		return color.NRGBA{R: 18, G: 19, B: 22, A: 255}
	case theme.ColorNameInputBorder:
		return color.NRGBA{R: 74, G: 52, B: 36, A: 255}
	case theme.ColorNamePrimary:
		return color.NRGBA{R: 241, G: 194, B: 124, A: 255}
	case theme.ColorNameSelection:
		return color.NRGBA{R: 72, G: 54, B: 38, A: 255}
	case theme.ColorNameSeparator:
		return color.NRGBA{R: 42, G: 36, B: 28, A: 255}
	case theme.ColorNameShadow:
		return color.NRGBA{R: 0, G: 0, B: 0, A: 150}
	default:
		return theme.DarkTheme().Color(name, variant)
	}
}

func (kratomixTheme) Font(style fyne.TextStyle) fyne.Resource {
	return theme.DarkTheme().Font(style)
}

func (kratomixTheme) Icon(name fyne.ThemeIconName) fyne.Resource {
	return theme.DarkTheme().Icon(name)
}

func (kratomixTheme) Size(name fyne.ThemeSizeName) float32 {
	return theme.DarkTheme().Size(name)
}
