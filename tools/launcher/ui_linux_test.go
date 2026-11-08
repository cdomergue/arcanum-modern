package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestLauncherRendererCrashFallback(t *testing.T) {
	for _, mode := range []string{"crash", "error", "explicit", "both"} {
		t.Run(mode, func(t *testing.T) {
			root := t.TempDir()
			t.Setenv("SDL_RENDER_DRIVER", "")
			if mode == "explicit" {
				t.Setenv("SDL_RENDER_DRIVER", "vulkan")
			}
			t.Setenv("TEST_UI_MODE", mode)
			script := `#!/bin/sh
ulimit -c 0
printf '%s\n' "$SDL_RENDER_DRIVER" >> attempts
echo 'renderer diagnostic' >&2
if [ "$SDL_RENDER_DRIVER" = software ] && [ "$TEST_UI_MODE" != both ]; then exit 0; fi
if [ "$TEST_UI_MODE" = error ]; then exit 1; fi
kill -SEGV $$
`
			path := filepath.Join(root, "ArcanumModern/launcher/renaissance-launcher-ui")
			put(t, root, "ArcanumModern/launcher/renaissance-launcher-ui", []byte(script))
			if err := os.Chmod(path, 0755); err != nil {
				t.Fatal(err)
			}
			a := newApplication(root, nil, true)
			err := showLauncher(a, preferences{})
			if (err == nil) != (mode == "crash") {
				t.Fatalf("unexpected result: %v", err)
			}
			attempts, _ := os.ReadFile(filepath.Join(root, "attempts"))
			want := 1
			if mode == "crash" || mode == "both" {
				want = 2
			}
			if strings.Count(string(attempts), "\n") != want {
				t.Fatalf("attempts: %q", attempts)
			}
			log, _ := os.ReadFile(filepath.Join(root, "ArcanumModern/launcher-ui.log"))
			for _, entry := range []string{"process_start", "renderer diagnostic", "process_exit", "elapsed_ms="} {
				if !strings.Contains(string(log), entry) {
					t.Fatalf("missing %s in %s", entry, log)
				}
			}
		})
	}
}
