package main

import (
	"os"
	"path/filepath"
	"testing"
)

func TestNormalizeChannel(t *testing.T) {
	if got := normalizedChannel("unstable"); got != "prerelease" {
		t.Fatalf("normalizedChannel(unstable) = %q, want prerelease", got)
	}
	if got := normalizedChannel("stable"); got != "stable" {
		t.Fatalf("normalizedChannel(stable) = %q, want stable", got)
	}
	if got := normalizedChannel("unknown"); got != "stable" {
		t.Fatalf("normalizedChannel(unknown) = %q, want stable", got)
	}
}

func TestFindPrereleaseManifestURL(t *testing.T) {
	releases := []githubRelease{
		{Prerelease: false, Assets: []githubAsset{{Name: "manifest.json", BrowserDownloadURL: "stable"}}},
		{Prerelease: true, Assets: []githubAsset{{Name: "notes.txt", BrowserDownloadURL: "ignored"}}},
		{Prerelease: true, Assets: []githubAsset{{Name: "manifest.json", BrowserDownloadURL: "unstable"}}},
	}

	got, ok := findPrereleaseManifestURL(releases)
	if !ok {
		t.Fatal("expected prerelease manifest URL")
	}
	if got != "unstable" {
		t.Fatalf("manifest URL = %q, want unstable", got)
	}
}

func TestReadBundleVersionFromInfoPlist(t *testing.T) {
	bundle := filepath.Join(t.TempDir(), "Kratomix Test.component")
	contents := filepath.Join(bundle, "Contents")
	if err := os.MkdirAll(contents, 0o755); err != nil {
		t.Fatal(err)
	}
	plist := `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleShortVersionString</key>
  <string>0.2.1</string>
  <key>CFBundleVersion</key>
  <string>99</string>
</dict>
</plist>`
	if err := os.WriteFile(filepath.Join(contents, "Info.plist"), []byte(plist), 0o644); err != nil {
		t.Fatal(err)
	}

	version, ok := readBundleVersion(bundle)
	if !ok {
		t.Fatal("expected version to be found")
	}
	if version != "0.2.1" {
		t.Fatalf("version = %q, want 0.2.1", version)
	}
}

func TestFormatInstallStateShowsUpdates(t *testing.T) {
	state := pluginInstallState{
		AUVersion:   "0.2.1",
		VST3Version: "0.2.2",
	}

	got := formatInstallState(state, "0.2.2", []string{"au", "vst3"})
	want := "AU 0.2.1 -> 0.2.2 update available | VST3 0.2.2 installed"
	if got != want {
		t.Fatalf("formatInstallState() = %q, want %q", got, want)
	}
}

func TestFormatInstallStateShowsNotInstalled(t *testing.T) {
	got := formatInstallState(pluginInstallState{}, "0.2.2", []string{"au", "vst3"})
	want := "Not installed"
	if got != want {
		t.Fatalf("formatInstallState() = %q, want %q", got, want)
	}
}

func TestFormatInstallStateShowsMissingFormat(t *testing.T) {
	state := pluginInstallState{AUVersion: "0.2.2"}

	got := formatInstallState(state, "0.2.2", []string{"au", "vst3"})
	want := "AU 0.2.2 installed | VST3 not installed"
	if got != want {
		t.Fatalf("formatInstallState() = %q, want %q", got, want)
	}
}
