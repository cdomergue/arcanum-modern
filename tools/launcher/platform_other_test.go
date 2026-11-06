//go:build !windows

package main

import (
	"archive/zip"
	"context"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"testing"
	"time"
)

func TestPosixNativeArchiveDetection(t *testing.T) {
	binary := os.Getenv("RENAISSANCE_LAUNCHER_BINARY")
	var data []byte
	var err error
	if binary == "" {
		archive := os.Getenv("RENAISSANCE_LAUNCHER_PACKAGE")
		if archive == "" {
			t.Skip("set RENAISSANCE_LAUNCHER_BINARY or RENAISSANCE_LAUNCHER_PACKAGE to test the compiled launcher")
		}
		z, err := zip.OpenReader(archive)
		if err != nil {
			t.Fatal(err)
		}
		defer z.Close()
		reader, err := z.Open("renaissance-launcher")
		if err != nil {
			t.Fatal(err)
		}
		data, err = io.ReadAll(reader)
		reader.Close()
		if err != nil {
			t.Fatal(err)
		}
	} else {
		data, err = os.ReadFile(binary)
		if err != nil {
			t.Fatal(err)
		}
	}
	// Stay offline: reaching the helper proves the real entry point accepted
	// the installation. The helper immediately exits and cancels preparation.
	t.Setenv("HTTPS_PROXY", "http://127.0.0.1:1")
	t.Setenv("NO_PROXY", "")
	t.Setenv("XDG_CACHE_HOME", t.TempDir())
	for _, name := range []string{"", "arcanum1.dat", "Arcanum1.dat", "ARCANUM1.DAT", "aRcAnUm1.DaT"} {
		t.Run(name, func(t *testing.T) {
			root := t.TempDir()
			launcher := filepath.Join(root, "renaissance-launcher")
			put(t, root, "renaissance-launcher", data)
			put(t, root, gameRelative(), []byte("unused game fixture"))
			if name != "" {
				put(t, root, name, []byte("archive fixture"))
			}
			helper := "ArcanumModern/launcher/renaissance-launcher-ui"
			put(t, root, helper, []byte("#!/bin/sh\nprintf reached > helper-reached\n"))
			for _, path := range []string{launcher, filepath.Join(root, helper)} {
				if err := os.Chmod(path, 0755); err != nil {
					t.Fatal(err)
				}
			}
			ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
			defer cancel()
			output, err := exec.CommandContext(ctx, launcher).CombinedOutput()
			if err != nil {
				t.Fatalf("launcher failed: %v: %s", err, output)
			}
			if exists(filepath.Join(root, "helper-reached")) != (name != "") {
				t.Fatalf("wrong archive detection for %q: %s", name, output)
			}
			if name == "" && !strings.Contains(string(output), "arcanum1.dat") {
				t.Fatalf("missing installation diagnostic: %s", output)
			}
		})
	}
}

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
