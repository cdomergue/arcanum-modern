//go:build !windows

package main

import (
	"os"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
)

func TestPosixGameHandoffAndRendererRecovery(t *testing.T) {
	for _, crash := range []bool{false, true} {
		root := t.TempDir()
		script := "#!/bin/sh\nprintf '%s\\n' \"$PWD\" \"$@\" >> launched\n"
		if crash && runtime.GOOS == "linux" {
			script += "if [ \"$SDL_RENDER_DRIVER\" = vulkan ]; then exit 139; fi\n"
		}
		put(t, root, gameRelative(), []byte(script))
		if e := os.Chmod(filepath.Join(root, gameRelative()), 0755); e != nil {
			t.Fatal(e)
		}
		t.Setenv("SDL_RENDER_DRIVER", "")
		a := application{root: root, args: []string{"-window", "argument with spaces"}}
		if e := a.launch(); e != nil {
			t.Fatal(e)
		}
		b, e := os.ReadFile(filepath.Join(root, "launched"))
		if e != nil {
			t.Fatal(e)
		}
		if !strings.Contains(string(b), root+"\n--renaissance-launcher\n-window\nargument with spaces\n") {
			t.Fatal(string(b))
		}
		expected := 1
		if crash && runtime.GOOS == "linux" {
			expected = 2
		}
		if strings.Count(string(b), "--renaissance-launcher") != expected {
			t.Fatal("wrong retry count")
		}
	}
}
func TestPosixExecutableModesSurviveUpdateAndRecovery(t *testing.T) {
	root := t.TempDir()
	fixture(t, root, "2.0.1")
	game := filepath.Join(root, gameRelative())
	if e := os.Chmod(game, 0755); e != nil {
		t.Fatal(e)
	}
	backup := filepath.Join(t.TempDir(), "backup")
	if e := backupFile(game, backup); e != nil {
		t.Fatal(e)
	}
	info, e := os.Stat(backup)
	if e != nil || info.Mode().Perm() != 0755 {
		t.Fatal("backup lost executable mode")
	}
}
