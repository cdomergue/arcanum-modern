package main

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func TestLinuxRendererFallbackConditions(t *testing.T) {
	for _, test := range []struct {
		mode, renderer string
		calls          int
		success        bool
	}{
		{"crash", "", 2, true}, {"ready", "", 1, false}, {"error", "", 1, false}, {"ok", "", 1, true},
		{"crash", "vulkan", 1, false}, {"ok", "software", 1, true}, {"both", "", 2, false},
	} {
		t.Run(test.mode+test.renderer, func(t *testing.T) {
			root := t.TempDir()
			t.Setenv("TEST_MODE", test.mode)
			t.Setenv("SDL_RENDER_DRIVER", test.renderer)
			script := `#!/bin/sh
ulimit -c 0
echo "$SDL_RENDER_DRIVER" >> attempts
if [ "$SDL_RENDER_DRIVER" = software ] && [ "$TEST_MODE" != both ]; then exit 0; fi
case "$TEST_MODE" in
ready) echo 'Arcanum Modern: startup complete'; kill -SEGV $$ ;;
crash|both) kill -SEGV $$ ;;
error) exit 24 ;;
esac
`
			put(t, root, gameRelative(), []byte(script))
			_ = os.Chmod(filepath.Join(root, gameRelative()), 0755)
			e := runGame(root, []string{"--renaissance-launcher"})
			if (e == nil) != test.success {
				t.Fatalf("exit: %v", e)
			}
			calls, _ := os.ReadFile(filepath.Join(root, "attempts"))
			if strings.Count(string(calls), "\n") != test.calls {
				t.Fatal(string(calls))
			}
		})
	}
}
