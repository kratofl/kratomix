package main

import (
	"archive/zip"
	"encoding/json"
	"encoding/xml"
	"errors"
	"flag"
	"fmt"
	"image"
	"image/color"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"gioui.org/app"
	"gioui.org/font"
	"gioui.org/layout"
	"gioui.org/op"
	"gioui.org/op/clip"
	"gioui.org/op/paint"
	"gioui.org/text"
	"gioui.org/unit"
	"gioui.org/widget"
	"gioui.org/widget/material"
)

const (
	defaultManifestURL = "https://github.com/kratofl/kratomix/releases/latest/download/manifest.json"
	githubReleasesURL  = "https://api.github.com/repos/kratofl/kratomix/releases?per_page=20"
)

var httpClient = &http.Client{Timeout: 12 * time.Second}

var (
	shellColor  = color.NRGBA{R: 8, G: 9, B: 11, A: 255}
	panelColor  = color.NRGBA{R: 18, G: 19, B: 22, A: 255}
	cardColor   = color.NRGBA{R: 27, G: 24, B: 22, A: 255}
	lineColor   = color.NRGBA{R: 74, G: 52, B: 36, A: 255}
	accentColor = color.NRGBA{R: 241, G: 194, B: 124, A: 255}
	textColor   = color.NRGBA{R: 250, G: 231, B: 202, A: 255}
	mutedColor  = color.NRGBA{R: 160, G: 146, B: 120, A: 255}
)

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

type cliOptions struct {
	headless bool
	list     bool
	manifest string
	plugins  string
	formats  string
	scope    string
	channel  string
}

type installJob struct {
	source string
	target string
	format string
	name   string
}

type pluginInstallState struct {
	AUVersion   string
	VST3Version string
}

type githubRelease struct {
	Prerelease bool          `json:"prerelease"`
	Assets     []githubAsset `json:"assets"`
}

type githubAsset struct {
	Name               string `json:"name"`
	BrowserDownloadURL string `json:"browser_download_url"`
}

type installerUI struct {
	win *app.Window
	th  *material.Theme

	mu             sync.Mutex
	current        manifest
	manifestSource string
	manifestLoaded bool
	status         string
	logLines       []string
	installing     bool

	activeTab      string
	manifestEditor widget.Editor
	pluginChecks   map[string]*widget.Bool
	scopeChoice    widget.Enum
	channelChoice  widget.Enum
	auCheck        widget.Bool
	vst3Check      widget.Bool
	pluginList     layout.List
	logList        layout.List

	installButton   widget.Clickable
	refreshButton   widget.Clickable
	settingsButton  widget.Clickable
	installTab      widget.Clickable
	settingsTab     widget.Clickable
	selectAllButton widget.Clickable
	clearButton     widget.Clickable
}

func parseCLIOptions() cliOptions {
	options := cliOptions{manifest: defaultManifestURL, plugins: "all", formats: "au,vst3", scope: "system", channel: "stable"}
	flag.BoolVar(&options.headless, "headless", false, "run without the GUI")
	flag.BoolVar(&options.list, "list", false, "list available plugins and exit")
	flag.StringVar(&options.manifest, "manifest", defaultManifestURL, "release manifest URL or local path")
	flag.StringVar(&options.plugins, "plugins", "all", "comma-separated plugin slugs or all")
	flag.StringVar(&options.formats, "formats", "au,vst3", "comma-separated formats: au,vst3")
	flag.StringVar(&options.scope, "scope", "system", "installation scope: system or user")
	flag.StringVar(&options.channel, "channel", "stable", "release channel: stable or prerelease")
	flag.Parse()
	return options
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

	go runGUI(options)
	app.Main()
}

func runGUI(options cliOptions) {
	win := new(app.Window)
	win.Option(app.Title("Kratomix Installer"), app.Decorated(true), app.Size(unit.Dp(920), unit.Dp(640)), app.MinSize(unit.Dp(760), unit.Dp(520)))

	ui := newInstallerUI(win, options)
	ui.startManifestLoad(options.manifest, true)

	var ops op.Ops
	for {
		switch event := win.Event().(type) {
		case app.DestroyEvent:
			os.Exit(0)
		case app.FrameEvent:
			gtx := app.NewContext(&ops, event)
			ui.layout(gtx)
			event.Frame(gtx.Ops)
		}
	}
}

func newInstallerUI(win *app.Window, options cliOptions) *installerUI {
	th := material.NewTheme()
	th.Palette = material.Palette{
		Bg:         shellColor,
		Fg:         textColor,
		ContrastBg: accentColor,
		ContrastFg: color.NRGBA{R: 18, G: 14, B: 11, A: 255},
	}
	th.TextSize = unit.Sp(15)

	ui := &installerUI{
		win:          win,
		th:           th,
		status:       "Loading release manifest...",
		activeTab:    "install",
		pluginChecks: make(map[string]*widget.Bool),
		pluginList:   layout.List{Axis: layout.Vertical},
		logList:      layout.List{Axis: layout.Vertical},
	}
	ui.manifestEditor.SingleLine = true
	ui.manifestEditor.SetText(options.manifest)
	ui.auCheck.Value = true
	ui.vst3Check.Value = true
	ui.scopeChoice.Value = normalizedScope(options.scope)
	ui.channelChoice.Value = normalizedChannel(options.channel)
	return ui
}

func (ui *installerUI) layout(gtx layout.Context) layout.Dimensions {
	ui.handleClicks(gtx)
	paint.FillShape(gtx.Ops, shellColor, clip.Rect{Max: gtx.Constraints.Max}.Op())

	return layout.UniformInset(unit.Dp(18)).Layout(gtx, func(gtx layout.Context) layout.Dimensions {
		return layout.Flex{Axis: layout.Vertical}.Layout(gtx,
			layout.Rigid(ui.header),
			layout.Rigid(layout.Spacer{Height: unit.Dp(14)}.Layout),
			layout.Rigid(ui.tabs),
			layout.Rigid(layout.Spacer{Height: unit.Dp(12)}.Layout),
			layout.Flexed(1, ui.body),
		)
	})
}

func (ui *installerUI) handleClicks(gtx layout.Context) {
	if ui.installTab.Clicked(gtx) {
		ui.activeTab = "install"
	}
	if ui.settingsTab.Clicked(gtx) {
		ui.activeTab = "settings"
	}
	if ui.refreshButton.Clicked(gtx) {
		ui.startManifestLoad(strings.TrimSpace(ui.manifestEditor.Text()), true)
	}
	if ui.selectAllButton.Clicked(gtx) {
		for _, check := range ui.pluginChecks {
			check.Value = true
		}
	}
	if ui.clearButton.Clicked(gtx) {
		for _, check := range ui.pluginChecks {
			check.Value = false
		}
	}
	if ui.installButton.Clicked(gtx) {
		ui.startInstall()
	}
}

func (ui *installerUI) header(gtx layout.Context) layout.Dimensions {
	return layout.Flex{Axis: layout.Horizontal, Alignment: layout.Middle}.Layout(gtx,
		layout.Flexed(1, func(gtx layout.Context) layout.Dimensions {
			return layout.Flex{Axis: layout.Vertical}.Layout(gtx,
				layout.Rigid(func(gtx layout.Context) layout.Dimensions {
					label := material.H3(ui.th, "Kratomix")
					label.Color = color.NRGBA{R: 246, G: 226, B: 192, A: 255}
					label.Font.Typeface = font.Typeface("Snell Roundhand, Apple Chancery, cursive")
					label.Font.Style = font.Italic
					label.Font.Weight = font.Bold
					return label.Layout(gtx)
				}),
				layout.Rigid(func(gtx layout.Context) layout.Dimensions {
					label := material.Body1(ui.th, "Installer")
					label.Color = accentColor
					label.Font.Weight = font.Bold
					return label.Layout(gtx)
				}),
			)
		}),
		layout.Rigid(func(gtx layout.Context) layout.Dimensions {
			label := material.Body1(ui.th, "Logic Pro only for now")
			label.Color = accentColor
			label.Font.Weight = font.Bold
			label.Alignment = text.End
			return label.Layout(gtx)
		}),
	)
}

func (ui *installerUI) tabs(gtx layout.Context) layout.Dimensions {
	return layout.Flex{Axis: layout.Horizontal}.Layout(gtx,
		layout.Rigid(ui.tabButton(&ui.installTab, "Install", ui.activeTab == "install")),
		layout.Rigid(layout.Spacer{Width: unit.Dp(8)}.Layout),
		layout.Rigid(ui.tabButton(&ui.settingsTab, "Settings", ui.activeTab == "settings")),
	)
}

func (ui *installerUI) tabButton(button *widget.Clickable, label string, active bool) layout.Widget {
	return func(gtx layout.Context) layout.Dimensions {
		style := material.Button(ui.th, button, label)
		style.CornerRadius = unit.Dp(7)
		if active {
			style.Background = accentColor
			style.Color = color.NRGBA{R: 18, G: 14, B: 11, A: 255}
		} else {
			style.Background = cardColor
			style.Color = textColor
		}
		return style.Layout(gtx)
	}
}

func (ui *installerUI) body(gtx layout.Context) layout.Dimensions {
	return ui.panel(gtx, func(gtx layout.Context) layout.Dimensions {
		if ui.activeTab == "settings" {
			return ui.settings(gtx)
		}
		return ui.install(gtx)
	})
}

func (ui *installerUI) install(gtx layout.Context) layout.Dimensions {
	return layout.UniformInset(unit.Dp(18)).Layout(gtx, func(gtx layout.Context) layout.Dimensions {
		return layout.Flex{Axis: layout.Vertical}.Layout(gtx,
			layout.Rigid(ui.controls),
			layout.Rigid(layout.Spacer{Height: unit.Dp(12)}.Layout),
			layout.Flexed(1, ui.plugins),
			layout.Rigid(layout.Spacer{Height: unit.Dp(12)}.Layout),
			layout.Rigid(ui.footer),
		)
	})
}

func (ui *installerUI) controls(gtx layout.Context) layout.Dimensions {
	return layout.Flex{Axis: layout.Horizontal, Alignment: layout.Middle}.Layout(gtx,
		layout.Rigid(func(gtx layout.Context) layout.Dimensions {
			label := material.Body1(ui.th, "Formats")
			label.Color = mutedColor
			label.Font.Weight = font.Bold
			return label.Layout(gtx)
		}),
		layout.Rigid(layout.Spacer{Width: unit.Dp(14)}.Layout),
		layout.Rigid(material.CheckBox(ui.th, &ui.auCheck, "AU").Layout),
		layout.Rigid(layout.Spacer{Width: unit.Dp(8)}.Layout),
		layout.Rigid(material.CheckBox(ui.th, &ui.vst3Check, "VST3").Layout),
		layout.Flexed(1, layout.Spacer{}.Layout),
		layout.Rigid(ui.smallButton(&ui.selectAllButton, "Select All")),
		layout.Rigid(layout.Spacer{Width: unit.Dp(8)}.Layout),
		layout.Rigid(ui.smallButton(&ui.clearButton, "Clear")),
	)
}

func (ui *installerUI) plugins(gtx layout.Context) layout.Dimensions {
	ui.mu.Lock()
	plugins := append([]plugin(nil), ui.current.Plugins...)
	loaded := ui.manifestLoaded
	ui.mu.Unlock()

	if !loaded {
		return ui.emptyState(gtx, "Loading plugins...")
	}
	if len(plugins) == 0 {
		return ui.emptyState(gtx, "No plugins in manifest.")
	}

	return ui.pluginList.Layout(gtx, len(plugins), func(gtx layout.Context, index int) layout.Dimensions {
		p := plugins[index]
		check := ui.checkboxFor(p)
		return layout.Inset{Bottom: unit.Dp(10)}.Layout(gtx, func(gtx layout.Context) layout.Dimensions {
			return ui.card(gtx, func(gtx layout.Context) layout.Dimensions {
				return layout.Flex{Axis: layout.Horizontal, Alignment: layout.Middle}.Layout(gtx,
					layout.Flexed(1, func(gtx layout.Context) layout.Dimensions {
						return layout.Flex{Axis: layout.Vertical}.Layout(gtx,
							layout.Rigid(material.CheckBox(ui.th, check, p.Name).Layout),
							layout.Rigid(func(gtx layout.Context) layout.Dimensions {
								state := scanPluginInstallState(p, ui.scopeChoice.Value)
								label := material.Body1(ui.th, formatInstallState(state, p.Version, pluginFormats(p)))
								label.Color = mutedColor
								label.TextSize = unit.Sp(13)
								return label.Layout(gtx)
							}),
						)
					}),
					layout.Rigid(func(gtx layout.Context) layout.Dimensions {
						label := material.Body1(ui.th, availableFormatText(p))
						label.Color = accentColor
						label.Font.Weight = font.Bold
						return label.Layout(gtx)
					}),
				)
			})
		})
	})
}

func (ui *installerUI) footer(gtx layout.Context) layout.Dimensions {
	return layout.Flex{Axis: layout.Horizontal, Alignment: layout.Middle}.Layout(gtx,
		layout.Flexed(1, ui.mainHint),
		layout.Rigid(layout.Spacer{Width: unit.Dp(16)}.Layout),
		layout.Rigid(func(gtx layout.Context) layout.Dimensions {
			style := material.Button(ui.th, &ui.installButton, "Install Selected")
			style.CornerRadius = unit.Dp(8)
			style.Background = accentColor
			style.Color = color.NRGBA{R: 18, G: 14, B: 11, A: 255}
			return style.Layout(gtx)
		}),
	)
}

func (ui *installerUI) mainHint(gtx layout.Context) layout.Dimensions {
	ui.mu.Lock()
	status := ui.status
	scope := ui.scopeChoice.Value
	ui.mu.Unlock()
	if status == "" || strings.Contains(status, "loaded") || status == "Ready to install" {
		status = installLocationText(scope)
	}
	label := material.Body1(ui.th, status)
	label.Color = mutedColor
	label.TextSize = unit.Sp(13)
	return label.Layout(gtx)
}

func (ui *installerUI) logPanel(gtx layout.Context) layout.Dimensions {
	ui.mu.Lock()
	logs := append([]string(nil), ui.logLines...)
	ui.mu.Unlock()
	if len(logs) == 0 {
		logs = []string{"Ready."}
	}
	if len(logs) > 4 {
		logs = logs[len(logs)-4:]
	}
	return ui.card(gtx, func(gtx layout.Context) layout.Dimensions {
		return ui.logList.Layout(gtx, len(logs), func(gtx layout.Context, index int) layout.Dimensions {
			label := material.Body1(ui.th, logs[index])
			label.Color = mutedColor
			label.TextSize = unit.Sp(12)
			return label.Layout(gtx)
		})
	})
}

func (ui *installerUI) settings(gtx layout.Context) layout.Dimensions {
	return layout.UniformInset(unit.Dp(18)).Layout(gtx, func(gtx layout.Context) layout.Dimensions {
		return layout.Flex{Axis: layout.Vertical}.Layout(gtx,
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				label := material.H6(ui.th, "Release Channel")
				label.Color = textColor
				return label.Layout(gtx)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(8)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				return layout.Flex{Axis: layout.Horizontal, Alignment: layout.Middle}.Layout(gtx,
					layout.Rigid(material.RadioButton(ui.th, &ui.channelChoice, "stable", "Stable").Layout),
					layout.Rigid(layout.Spacer{Width: unit.Dp(12)}.Layout),
					layout.Rigid(material.RadioButton(ui.th, &ui.channelChoice, "prerelease", "Unstable").Layout),
				)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(8)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				label := material.Body1(ui.th, channelDescription(ui.channelChoice.Value))
				label.Color = mutedColor
				label.TextSize = unit.Sp(13)
				return label.Layout(gtx)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(18)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				label := material.H6(ui.th, "Install Location")
				label.Color = textColor
				return label.Layout(gtx)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(8)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				return layout.Flex{Axis: layout.Horizontal, Alignment: layout.Middle}.Layout(gtx,
					layout.Rigid(material.RadioButton(ui.th, &ui.scopeChoice, "system", "System-wide").Layout),
					layout.Rigid(layout.Spacer{Width: unit.Dp(12)}.Layout),
					layout.Rigid(material.RadioButton(ui.th, &ui.scopeChoice, "user", "User only").Layout),
				)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(8)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				label := material.Body1(ui.th, installLocationText(ui.scopeChoice.Value))
				label.Color = mutedColor
				label.TextSize = unit.Sp(13)
				return label.Layout(gtx)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(18)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				label := material.H6(ui.th, "Release Manifest")
				label.Color = textColor
				return label.Layout(gtx)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(10)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				return ui.card(gtx, func(gtx layout.Context) layout.Dimensions {
					return material.Editor(ui.th, &ui.manifestEditor, defaultManifestURL).Layout(gtx)
				})
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(12)}.Layout),
			layout.Rigid(ui.smallButton(&ui.refreshButton, "Reload Manifest")),
			layout.Rigid(layout.Spacer{Height: unit.Dp(18)}.Layout),
			layout.Rigid(func(gtx layout.Context) layout.Dimensions {
				ui.mu.Lock()
				source := ui.manifestSource
				ui.mu.Unlock()
				text := "The installer loads GitHub first and falls back to the bundled manifest when offline."
				if source != "" {
					text += "\nCurrent source: " + source
				}
				label := material.Body1(ui.th, text)
				label.Color = mutedColor
				return label.Layout(gtx)
			}),
			layout.Rigid(layout.Spacer{Height: unit.Dp(18)}.Layout),
			layout.Flexed(1, ui.logPanel),
		)
	})
}

func (ui *installerUI) checkboxFor(p plugin) *widget.Bool {
	check, ok := ui.pluginChecks[p.Slug]
	if !ok {
		check = &widget.Bool{Value: true}
		ui.pluginChecks[p.Slug] = check
	}
	return check
}

func (ui *installerUI) smallButton(button *widget.Clickable, text string) layout.Widget {
	return func(gtx layout.Context) layout.Dimensions {
		style := material.Button(ui.th, button, text)
		style.CornerRadius = unit.Dp(7)
		style.Background = cardColor
		style.Color = textColor
		style.Inset = layout.Inset{Top: unit.Dp(8), Bottom: unit.Dp(8), Left: unit.Dp(12), Right: unit.Dp(12)}
		return style.Layout(gtx)
	}
}

func (ui *installerUI) panel(gtx layout.Context, child layout.Widget) layout.Dimensions {
	return roundedBackground(gtx, panelColor, unit.Dp(10), func(gtx layout.Context) layout.Dimensions {
		return child(gtx)
	})
}

func (ui *installerUI) card(gtx layout.Context, child layout.Widget) layout.Dimensions {
	return roundedBackground(gtx, cardColor, unit.Dp(8), func(gtx layout.Context) layout.Dimensions {
		return layout.UniformInset(unit.Dp(14)).Layout(gtx, child)
	})
}

func (ui *installerUI) emptyState(gtx layout.Context, message string) layout.Dimensions {
	return layout.Center.Layout(gtx, func(gtx layout.Context) layout.Dimensions {
		label := material.Body1(ui.th, message)
		label.Color = mutedColor
		return label.Layout(gtx)
	})
}

func roundedBackground(gtx layout.Context, bg color.NRGBA, radius unit.Dp, child layout.Widget) layout.Dimensions {
	return layout.Background{}.Layout(gtx,
		func(gtx layout.Context) layout.Dimensions {
			defer clip.UniformRRect(image.Rectangle{Max: gtx.Constraints.Min}, gtx.Dp(radius)).Push(gtx.Ops).Pop()
			paint.Fill(gtx.Ops, bg)
			return layout.Dimensions{Size: gtx.Constraints.Min}
		},
		child,
	)
}

func (ui *installerUI) setStatus(status string) {
	ui.mu.Lock()
	ui.status = status
	ui.mu.Unlock()
	ui.win.Invalidate()
}

func (ui *installerUI) logf(format string, args ...any) {
	line := fmt.Sprintf(format, args...)
	ui.mu.Lock()
	ui.logLines = append(ui.logLines, line)
	ui.mu.Unlock()
	ui.win.Invalidate()
}

func (ui *installerUI) startManifestLoad(source string, fallback bool) {
	if strings.TrimSpace(source) == "" {
		source = defaultManifestURL
	}
	ui.setStatus("Loading release manifest...")
	go func() {
		ui.mu.Lock()
		channel := ui.channelChoice.Value
		ui.mu.Unlock()
		m, loadedFrom, err := loadManifestForChannel(source, channel, fallback)
		ui.mu.Lock()
		defer ui.mu.Unlock()
		if err != nil {
			ui.status = "No release manifest loaded"
			ui.logLines = append(ui.logLines, "Manifest load failed: "+err.Error())
		} else {
			ui.current = m
			ui.manifestSource = loadedFrom
			ui.manifestLoaded = true
			ui.status = "Ready to install"
			ui.logLines = append(ui.logLines, fmt.Sprintf("Loaded %d plugins from %s", len(m.Plugins), loadedFrom))
		}
		ui.win.Invalidate()
	}()
}

func (ui *installerUI) startInstall() {
	ui.mu.Lock()
	if ui.installing {
		ui.mu.Unlock()
		return
	}
	selected := make([]plugin, 0)
	for _, p := range ui.current.Plugins {
		if check := ui.pluginChecks[p.Slug]; check != nil && check.Value {
			selected = append(selected, p)
		}
	}
	formats := selectedFormats(ui.auCheck.Value, ui.vst3Check.Value)
	scope := normalizedScope(ui.scopeChoice.Value)
	ui.installing = true
	ui.status = "Installing..."
	ui.mu.Unlock()

	if len(selected) == 0 {
		ui.setInstallFinished("Select at least one plugin.")
		return
	}
	if len(formats) == 0 {
		ui.setInstallFinished("Select at least one format.")
		return
	}

	go func() {
		tempDir, err := os.MkdirTemp("", "kratomix-installer-*")
		if err != nil {
			ui.setInstallFinished(err.Error())
			return
		}
		defer os.RemoveAll(tempDir)

		jobs := make([]installJob, 0, len(selected)*len(formats))
		for _, p := range selected {
			ui.logf("Preparing %s", p.Name)
			pluginJobs, err := preparePluginInstall(tempDir, p, formats, scope, ui.logf)
			if err != nil {
				ui.setInstallFinished("Install failed: " + err.Error())
				return
			}
			jobs = append(jobs, pluginJobs...)
		}
		if err := installJobs(jobs, scope, ui.logf); err != nil {
			ui.setInstallFinished("Install failed: " + err.Error())
			return
		}
		ui.setInstallFinished("Install complete. Restart your DAW.")
	}()
}

func (ui *installerUI) setInstallFinished(status string) {
	ui.mu.Lock()
	ui.status = status
	ui.logLines = append(ui.logLines, status)
	ui.installing = false
	ui.mu.Unlock()
	ui.win.Invalidate()
}

func runHeadless(options cliOptions) error {
	m, source, err := loadManifestForChannel(options.manifest, options.channel, true)
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
			state := scanPluginInstallState(p, options.scope)
			fmt.Printf("%s\t%s\t%s\t%s\n", p.Slug, p.Version, availableFormatText(p), formatInstallState(state, p.Version, pluginFormats(p)))
		}
		return nil
	}

	formats := parseCSV(options.formats)
	if len(formats) == 0 {
		return errors.New("select at least one format")
	}
	scope := normalizedScope(options.scope)

	tempDir, err := os.MkdirTemp("", "kratomix-installer-*")
	if err != nil {
		return err
	}
	defer os.RemoveAll(tempDir)

	logf := func(format string, args ...any) {
		fmt.Printf(format+"\n", args...)
	}

	jobs := make([]installJob, 0, len(selected)*len(formats))
	for _, p := range selected {
		pluginJobs, err := preparePluginInstall(tempDir, p, formats, scope, logf)
		if err != nil {
			return err
		}
		jobs = append(jobs, pluginJobs...)
	}
	if err := installJobs(jobs, scope, logf); err != nil {
		return err
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
	formats := pluginFormats(p)
	if len(formats) == 0 {
		return "No formats"
	}

	labels := make([]string, 0, len(formats))
	for _, format := range formats {
		labels = append(labels, strings.ToUpper(format))
	}
	return strings.Join(labels, " + ")
}

func pluginFormats(p plugin) []string {
	formats := make([]string, 0, 2)
	if p.Formats["au"] {
		formats = append(formats, "au")
	}
	if p.Formats["vst3"] {
		formats = append(formats, "vst3")
	}
	return formats
}

func scanPluginInstallState(p plugin, scope string) pluginInstallState {
	var state pluginInstallState
	if p.Formats["au"] {
		if base, err := installBasePath("au", scope); err == nil {
			if version, ok := readBundleVersion(filepath.Join(base, p.Name+".component")); ok {
				state.AUVersion = version
			}
		}
	}
	if p.Formats["vst3"] {
		if base, err := installBasePath("vst3", scope); err == nil {
			if version, ok := readBundleVersion(filepath.Join(base, p.Name+".vst3")); ok {
				state.VST3Version = version
			}
		}
	}
	return state
}

func formatInstallState(state pluginInstallState, latest string, formats []string) string {
	if state.AUVersion == "" && state.VST3Version == "" {
		return "Not installed"
	}
	parts := make([]string, 0, len(formats))
	for _, format := range formats {
		switch format {
		case "au":
			parts = append(parts, formatSingleInstallState("AU", state.AUVersion, latest))
		case "vst3":
			parts = append(parts, formatSingleInstallState("VST3", state.VST3Version, latest))
		}
	}
	return strings.Join(parts, " | ")
}

func formatSingleInstallState(format string, installed string, latest string) string {
	if installed == "" {
		return fmt.Sprintf("%s not installed", format)
	}
	if compareVersions(installed, latest) < 0 {
		return fmt.Sprintf("%s %s -> %s update available", format, installed, latest)
	}
	return fmt.Sprintf("%s %s installed", format, installed)
}

func compareVersions(left string, right string) int {
	leftParts := versionParts(left)
	rightParts := versionParts(right)
	maxParts := len(leftParts)
	if len(rightParts) > maxParts {
		maxParts = len(rightParts)
	}
	for i := 0; i < maxParts; i++ {
		var leftValue int
		var rightValue int
		if i < len(leftParts) {
			leftValue = leftParts[i]
		}
		if i < len(rightParts) {
			rightValue = rightParts[i]
		}
		if leftValue < rightValue {
			return -1
		}
		if leftValue > rightValue {
			return 1
		}
	}
	return strings.Compare(left, right)
}

func versionParts(version string) []int {
	fields := strings.FieldsFunc(version, func(r rune) bool {
		return r < '0' || r > '9'
	})
	parts := make([]int, 0, len(fields))
	for _, field := range fields {
		if field == "" {
			continue
		}
		value, err := strconv.Atoi(field)
		if err == nil {
			parts = append(parts, value)
		}
	}
	return parts
}

func readBundleVersion(bundlePath string) (string, bool) {
	file, err := os.Open(filepath.Join(bundlePath, "Contents", "Info.plist"))
	if err != nil {
		return "", false
	}
	defer file.Close()

	decoder := xml.NewDecoder(file)
	var lastKey string
	for {
		token, err := decoder.Token()
		if err != nil {
			break
		}
		start, ok := token.(xml.StartElement)
		if !ok {
			continue
		}
		switch start.Name.Local {
		case "key":
			var key string
			if err := decoder.DecodeElement(&key, &start); err == nil {
				lastKey = key
			}
		case "string":
			var value string
			if err := decoder.DecodeElement(&value, &start); err == nil &&
				(lastKey == "CFBundleShortVersionString" || lastKey == "CFBundleVersion") &&
				strings.TrimSpace(value) != "" {
				return strings.TrimSpace(value), true
			}
		}
	}
	return "", false
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

func loadManifestForChannel(source string, channel string, useFallback bool) (manifest, string, error) {
	if normalizedChannel(channel) == "prerelease" && strings.TrimSpace(source) == defaultManifestURL {
		prereleaseManifest, err := latestPrereleaseManifestURL()
		if err == nil {
			return loadManifestWithFallback(prereleaseManifest, useFallback)
		}
		if !useFallback {
			return manifest{}, "", err
		}
		m, loadedFrom, fallbackErr := loadManifestWithFallback(source, useFallback)
		if fallbackErr != nil {
			return manifest{}, "", fmt.Errorf("could not load prerelease manifest: %v\nfallback failed: %w", err, fallbackErr)
		}
		return m, loadedFrom, nil
	}
	return loadManifestWithFallback(source, useFallback)
}

func latestPrereleaseManifestURL() (string, error) {
	resp, err := httpClient.Get(githubReleasesURL)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", fmt.Errorf("GitHub releases request failed: %s", resp.Status)
	}

	var releases []githubRelease
	if err := json.NewDecoder(resp.Body).Decode(&releases); err != nil {
		return "", err
	}
	if url, ok := findPrereleaseManifestURL(releases); ok {
		return url, nil
	}
	return "", errors.New("no prerelease manifest asset found")
}

func findPrereleaseManifestURL(releases []githubRelease) (string, bool) {
	for _, release := range releases {
		if !release.Prerelease {
			continue
		}
		for _, asset := range release.Assets {
			if asset.Name == "manifest.json" && strings.TrimSpace(asset.BrowserDownloadURL) != "" {
				return asset.BrowserDownloadURL, true
			}
		}
	}
	return "", false
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

func normalizedScope(scope string) string {
	switch strings.ToLower(strings.TrimSpace(scope)) {
	case "user":
		return "user"
	default:
		return "system"
	}
}

func normalizedChannel(channel string) string {
	switch strings.ToLower(strings.TrimSpace(channel)) {
	case "prerelease", "pre-release", "unstable", "beta":
		return "prerelease"
	default:
		return "stable"
	}
}

func channelDescription(channel string) string {
	if normalizedChannel(channel) == "prerelease" {
		return "Unstable installs the newest GitHub prerelease. Use it for testing builds."
	}
	return "Stable installs the latest normal GitHub release."
}

func installLocationText(scope string) string {
	if normalizedScope(scope) == "user" {
		return "Installs to ~/Library/Audio/Plug-Ins. No administrator password is needed."
	}
	return "Installs system-wide to /Library/Audio/Plug-Ins. macOS asks once for administrator access."
}

func preparePluginInstall(tempDir string, p plugin, formats []string, scope string, logf func(string, ...any)) ([]installJob, error) {
	if p.URL == "" {
		return nil, fmt.Errorf("%s has no download URL", p.Name)
	}

	zipPath := filepath.Join(tempDir, p.Asset)
	extractDir := filepath.Join(tempDir, p.Slug)

	logf("Downloading %s", p.URL)
	if err := downloadPluginAsset(zipPath, p); err != nil {
		return nil, err
	}

	logf("Extracting %s", p.Asset)
	if err := unzip(zipPath, extractDir); err != nil {
		return nil, err
	}

	jobs := make([]installJob, 0, len(formats))
	for _, format := range formats {
		if !p.Formats[format] {
			logf("Skipping %s for %s; format is not available", strings.ToUpper(format), p.Name)
			continue
		}
		job, err := makeInstallJob(extractDir, p.Name, format, scope)
		if err != nil {
			return nil, err
		}
		jobs = append(jobs, job)
	}

	return jobs, nil
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

func makeInstallJob(extractDir string, productName string, format string, scope string) (installJob, error) {
	var source string
	var target string

	switch format {
	case "au":
		source = filepath.Join(extractDir, "AU", productName+".component")
	case "vst3":
		source = filepath.Join(extractDir, "VST3", productName+".vst3")
	default:
		return installJob{}, fmt.Errorf("unsupported format: %s", format)
	}

	base, err := installBasePath(format, scope)
	if err != nil {
		return installJob{}, err
	}
	target = filepath.Join(base, filepath.Base(source))

	if _, err := os.Stat(source); err != nil {
		return installJob{}, fmt.Errorf("missing %s bundle: %w", strings.ToUpper(format), err)
	}

	return installJob{source: source, target: target, format: format, name: productName}, nil
}

func installBasePath(format string, scope string) (string, error) {
	var root string
	if normalizedScope(scope) == "user" {
		home, err := os.UserHomeDir()
		if err != nil {
			return "", err
		}
		root = filepath.Join(home, "Library", "Audio", "Plug-Ins")
	} else {
		root = filepath.Join(string(os.PathSeparator), "Library", "Audio", "Plug-Ins")
	}

	switch format {
	case "au":
		return filepath.Join(root, "Components"), nil
	case "vst3":
		return filepath.Join(root, "VST3"), nil
	default:
		return "", fmt.Errorf("unsupported format: %s", format)
	}
}

func installJobs(jobs []installJob, scope string, logf func(string, ...any)) error {
	if len(jobs) == 0 {
		return errors.New("no installable formats selected")
	}
	if normalizedScope(scope) == "user" {
		for _, job := range jobs {
			logf("Installing %s to %s", job.source, job.target)
			if err := os.MkdirAll(filepath.Dir(job.target), 0o755); err != nil {
				return err
			}
			if err := runCommand("ditto", job.source, job.target); err != nil {
				return err
			}
			_ = runCommand("xattr", "-dr", "com.apple.quarantine", job.target)
		}
		return nil
	}

	var builder strings.Builder
	builder.WriteString("set -e\n")
	for _, job := range jobs {
		logf("Installing %s to %s", job.source, job.target)
		builder.WriteString("mkdir -p ")
		builder.WriteString(shellQuote(filepath.Dir(job.target)))
		builder.WriteString("\n")
		builder.WriteString("ditto ")
		builder.WriteString(shellQuote(job.source))
		builder.WriteString(" ")
		builder.WriteString(shellQuote(job.target))
		builder.WriteString("\n")
		builder.WriteString("xattr -dr com.apple.quarantine ")
		builder.WriteString(shellQuote(job.target))
		builder.WriteString(" >/dev/null 2>&1 || true\n")
	}

	if err := runPrivileged(builder.String()); err != nil {
		return err
	}
	return removeStaleUserScopeCopies(jobs, scope, logf)
}

func removeStaleUserScopeCopies(jobs []installJob, scope string, logf func(string, ...any)) error {
	if normalizedScope(scope) != "system" {
		return nil
	}

	for _, job := range jobs {
		userBase, err := installBasePath(job.format, "user")
		if err != nil {
			return err
		}
		staleTarget := filepath.Join(userBase, filepath.Base(job.target))
		if staleTarget == job.target {
			continue
		}

		if _, err := os.Stat(staleTarget); err != nil {
			if os.IsNotExist(err) {
				continue
			}
			return err
		}

		logf("Removing stale user-scope copy %s", staleTarget)
		if err := os.RemoveAll(staleTarget); err != nil {
			return err
		}
	}
	return nil
}

func runCommand(name string, args ...string) error {
	output, err := exec.Command(name, args...).CombinedOutput()
	if err != nil {
		return fmt.Errorf("%s failed: %w\n%s", name, err, strings.TrimSpace(string(output)))
	}
	return nil
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
