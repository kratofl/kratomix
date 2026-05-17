package main

import (
	"os"
	"path/filepath"
	"testing"
)

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
