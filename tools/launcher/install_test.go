package main

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func put(t *testing.T, root, name string, data []byte) {
	t.Helper()
	p := filepath.Join(root, filepath.FromSlash(name))
	if e := os.MkdirAll(filepath.Dir(p), 0755); e != nil {
		t.Fatal(e)
	}
	if e := os.WriteFile(p, data, 0600); e != nil {
		t.Fatal(e)
	}
}
func fixture(t *testing.T, root, ver string) manifest {
	t.Helper()
	m := manifest{Version: ver, Format: 1, Target: targetName(), Hashes: map[string]string{}}
	for _, name := range []string{gameRelative(), launcherRelative(), playRelative(), "modern-ui/Book.ttf"} {
		b := []byte(ver + " " + name)
		put(t, root, name, b)
		h := sha256.Sum256(b)
		m.Hashes[name] = hex.EncodeToString(h[:])
	}
	b, _ := json.Marshal(m)
	put(t, root, manifestName, b)
	return m
}
func zipFixture(t *testing.T, dir string, extra map[string]string, mutate func(*manifest)) string {
	t.Helper()
	source := t.TempDir()
	m := fixture(t, source, "2.0.2")
	for n, b := range extra {
		h := sha256.Sum256([]byte(b))
		m.Hashes[n] = hex.EncodeToString(h[:])
	}
	if mutate != nil {
		mutate(&m)
	}
	raw, _ := json.Marshal(m)
	name := filepath.Join(dir, "test.zip")
	f, e := os.Create(name)
	if e != nil {
		t.Fatal(e)
	}
	z := zip.NewWriter(f)
	for n := range m.Hashes {
		w, e := z.Create(n)
		if e != nil {
			t.Fatal(e)
		}
		b, _ := os.ReadFile(filepath.Join(source, n))
		if x, ok := extra[n]; ok {
			b = []byte(x)
		}
		_, _ = w.Write(b)
	}
	w, _ := z.Create(manifestName)
	_, _ = w.Write(raw)
	if e = z.Close(); e != nil {
		t.Fatal(e)
	}
	f.Close()
	return name
}
func TestUnpackValidation(t *testing.T) {
	for _, test := range []struct {
		name   string
		extra  map[string]string
		mutate func(*manifest)
		ok     bool
	}{
		{name: "valid", ok: true},
		{name: "save", extra: map[string]string{"modules/Arcanum/Save/SlotAuto.tfaf": "private"}},
		{name: "settings", extra: map[string]string{"modern-ui/settings.ini": "private"}},
		{name: "portrait", extra: map[string]string{"data/art/portrait/custom.bmp": "private"}},
		{name: "traversal", extra: map[string]string{"ArcanumModern/docs/../../escaped": "bad"}},
		{name: "case collision", extra: map[string]string{"ArcanumModern/docs/A.txt": "a", "ArcanumModern/docs/a.txt": "b"}},
		{name: "full game", mutate: func(m *manifest) { m.IncludesGameData = true }},
		{name: "wrong version", mutate: func(m *manifest) { m.Version = "9.0.0" }},
		{name: "wrong platform", mutate: func(m *manifest) { m.Target = "linux" }},
		{name: "wrong hash", mutate: func(m *manifest) { m.Hashes[gameRelative()] = strings.Repeat("0", 64) }},
		{name: "no launcher", mutate: func(m *manifest) { delete(m.Hashes, launcherRelative()) }},
	} {
		t.Run(test.name, func(t *testing.T) {
			dir := t.TempDir()
			_, e := unpack(zipFixture(t, dir, test.extra, test.mutate), filepath.Join(dir, "payload"), "v2.0.2")
			if (e == nil) != test.ok {
				t.Fatalf("got %v, success expected %v", e, test.ok)
			}
		})
	}
}
func TestSafePaths(t *testing.T) {
	for _, name := range []string{"../x", "/absolute", "C:/x", "modern-ui\\Book.ttf", "ArcanumModern/docs/file:stream", "ArcanumModern/docs/CON.txt", "ArcanumModern/docs/end. ", "ArcanumModern/house/base/Save/slot", "ArcanumModern/launcher-release.json", "modern-ui/local-maps/a.png", "ArcanumModern/uap/active/ready"} {
		if allowed(name) {
			t.Errorf("allowed %s", name)
		}
	}
	root := t.TempDir()
	outside := t.TempDir()
	if e := os.Symlink(outside, filepath.Join(root, "ArcanumModern")); e == nil {
		if _, e = safePath(root, "ArcanumModern/docs/test.txt"); e == nil {
			t.Fatal("followed a link")
		}
	}
}
func TestInstallPreservesPersonalFiles(t *testing.T) {
	root := t.TempDir()
	old := fixture(t, root, "2.0.1")
	obsolete := "ArcanumModern/docs/old.md"
	put(t, root, obsolete, []byte("old"))
	h := sha256.Sum256([]byte("old"))
	old.Hashes[obsolete] = hex.EncodeToString(h[:])
	b, _ := json.Marshal(old)
	put(t, root, manifestName, b)
	personal := []string{"modules/Arcanum/Save/SlotAuto.tfaf", "arcanum.cfg", "modern-ui/settings.ini", "modern-ui/launcher.json", "modern-ui/uap-fr/active/ready", "data/art/portrait/custom.bmp", "data/rules/custom-background.mes", "ArcanumModern/world-updates/user/manifest.json"}
	for _, name := range personal {
		put(t, root, name, []byte("preserve"))
	}
	payload := t.TempDir()
	m := fixture(t, payload, "2.0.2")
	if e := install(root, payload, m, nil); e != nil {
		t.Fatal(e)
	}
	for _, name := range personal {
		b, e := os.ReadFile(filepath.Join(root, name))
		if e != nil || string(b) != "preserve" {
			t.Fatalf("changed %s", name)
		}
	}
	if exists(filepath.Join(root, obsolete)) {
		t.Fatal("obsolete file retained")
	}
	if exists(filepath.Join(root, transactionDir)) {
		t.Fatal("transaction not cleaned")
	}
	for name, want := range m.Hashes {
		got, e := fileHash(filepath.Join(root, name))
		if e != nil || got != want {
			t.Fatalf("bad installed file %s", name)
		}
	}
}
func TestRollbackAndInterruptedRecovery(t *testing.T) {
	for _, crash := range []bool{false, true} {
		for at := 0; at < 6; at++ {
			t.Run(strings.Join([]string{map[bool]string{false: "failure", true: "crash"}[crash], string(rune('0' + at))}, "-"), func(t *testing.T) {
				root := t.TempDir()
				old := fixture(t, root, "2.0.1")
				before, _ := os.ReadFile(filepath.Join(root, manifestName))
				payload := t.TempDir()
				next := fixture(t, payload, "2.0.2")
				// New resource tests removing partially installed additions during recovery.
				put(t, payload, "ArcanumModern/docs/new.md", []byte("new"))
				h := sha256.Sum256([]byte("new"))
				next.Hashes["ArcanumModern/docs/new.md"] = hex.EncodeToString(h[:])
				b, _ := json.Marshal(next)
				put(t, payload, manifestName, b)
				func() {
					defer func() { _ = recover() }()
					e := install(root, payload, next, func(i int) error {
						if i == at {
							if crash {
								panic("power interruption")
							}
							return errors.New("simulated write failure")
						}
						return nil
					})
					if !crash && e == nil {
						t.Error("expected failure")
					}
				}()
				if e := recoverInstall(root); e != nil {
					t.Fatal(e)
				}
				if e := recoverInstall(root); e != nil {
					t.Fatal(e)
				}
				for name, want := range old.Hashes {
					got, e := fileHash(filepath.Join(root, name))
					if e != nil || got != want {
						t.Fatalf("rollback failed %s: %v", name, e)
					}
				}
				after, _ := os.ReadFile(filepath.Join(root, manifestName))
				if string(before) != string(after) {
					t.Fatal("manifest not restored")
				}
				if exists(filepath.Join(root, "ArcanumModern/docs/new.md")) {
					t.Fatal("new file not rolled back")
				}
			})
		}
	}
}
func TestUnmanagedFileConflict(t *testing.T) {
	root := t.TempDir()
	fixture(t, root, "2.0.1")
	put(t, root, "ArcanumModern/docs/new.md", []byte("user file"))
	payload := t.TempDir()
	m := fixture(t, payload, "2.0.2")
	m.Hashes["ArcanumModern/docs/new.md"] = strings.Repeat("0", 64)
	if install(root, payload, m, nil) == nil {
		t.Fatal("overwrote unmanaged file")
	}
}
func TestModifiedObsoleteFileIsKept(t *testing.T) {
	root := t.TempDir()
	old := fixture(t, root, "2.0.1")
	old.Hashes["ArcanumModern/docs/old.md"] = strings.Repeat("0", 64)
	b, _ := json.Marshal(old)
	put(t, root, manifestName, b)
	put(t, root, "ArcanumModern/docs/old.md", []byte("custom"))
	payload := t.TempDir()
	m := fixture(t, payload, "2.0.2")
	if e := install(root, payload, m, nil); e != nil {
		t.Fatal(e)
	}
	b, _ = os.ReadFile(filepath.Join(root, "ArcanumModern/docs/old.md"))
	if string(b) != "custom" {
		t.Fatal("modified file lost")
	}
}
