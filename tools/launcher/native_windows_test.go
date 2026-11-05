package main

import (
	"encoding/json"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"syscall"
	"testing"
	"time"
)

func init() {
	if os.Getenv("RENAISSANCE_TEST_REDIRECT") == "1" && strings.EqualFold(filepath.Base(os.Args[0]), "renaissance-launcher.exe") {
		cwd, _ := os.Getwd()
		b, _ := json.Marshal(struct {
			Directory string
			Args      []string
		}{cwd, os.Args[1:]})
		_ = os.WriteFile(filepath.Join(cwd, "redirect-started.json"), b, 0600)
		os.Exit(0)
	}
	// A copied test executable stands in for the game. This runs before Go's
	// testing flags are parsed, so real game arguments can be checked verbatim.
	if os.Getenv("RENAISSANCE_TEST_GAME") == "1" && strings.EqualFold(filepath.Base(os.Args[0]), "arcanum-modern.exe") {
		cwd, _ := os.Getwd()
		b, _ := json.Marshal(struct {
			Directory string
			Args      []string
			Window    string
		}{cwd, os.Args[1:], os.Getenv("ARCANUM_IGNORE_DISPLAY_SETTINGS")})
		_ = os.WriteFile(filepath.Join(cwd, "game-started.json"), b, 0600)
		time.Sleep(800 * time.Millisecond)
		os.Exit(0)
	}
}
func TestNativeGameHandoff(t *testing.T) {
	root := t.TempDir()
	exe, _ := os.Executable()
	bytes, e := os.ReadFile(exe)
	if e != nil {
		t.Fatal(e)
	}
	put(t, root, "arcanum-modern.exe", bytes)
	t.Setenv("RENAISSANCE_TEST_GAME", "1")
	a := application{root: root, args: []string{"-window", "-geometry=1280x720", "argument with spaces"}}
	done := make(chan error, 1)
	go func() { done <- a.launch() }()
	deadline := time.Now().Add(5 * time.Second)
	for !exists(filepath.Join(root, "game-started.json")) && time.Now().Before(deadline) {
		time.Sleep(20 * time.Millisecond)
	}
	if !gameRunning(root) {
		t.Error("running game was not detected")
	}
	if e = <-done; e != nil {
		t.Fatal(e)
	}
	var result struct {
		Directory string
		Args      []string
		Window    string
	}
	b, e := os.ReadFile(filepath.Join(root, "game-started.json"))
	if e != nil {
		t.Fatal(e)
	}
	_ = json.Unmarshal(b, &result)
	if result.Directory != root || len(result.Args) != 4 || result.Args[0] != "--renaissance-launcher" || result.Args[3] != "argument with spaces" || result.Window != "1" {
		t.Fatalf("wrong launch context: %+v", result)
	}
}

func TestNativeEngineRedirect(t *testing.T) {
	binary := os.Getenv("RENAISSANCE_ENGINE_BINARY")
	if binary == "" {
		t.Skip("set RENAISSANCE_ENGINE_BINARY to test the real engine entry point")
	}
	root := t.TempDir()
	engine, e := os.ReadFile(binary)
	if e != nil {
		t.Fatal(e)
	}
	put(t, root, "arcanum-modern.exe", engine)
	for _, name := range []string{"avformat-62.dll", "avcodec-62.dll", "avutil-60.dll"} {
		b, e := os.ReadFile(filepath.Join(filepath.Dir(binary), name))
		if e != nil {
			b, e = os.ReadFile(filepath.Join(filepath.Dir(binary), "../ffmpeg/bin", name))
		}
		if e != nil {
			t.Fatal(e)
		}
		put(t, root, name, b)
	}
	exe, _ := os.Executable()
	launcher, e := os.ReadFile(exe)
	if e != nil {
		t.Fatal(e)
	}
	put(t, root, "renaissance-launcher.exe", launcher)
	cmd := exec.Command(filepath.Join(root, "arcanum-modern.exe"), "-window", "argument é with spaces")
	cmd.Dir = t.TempDir()
	cmd.Env = append(os.Environ(), "RENAISSANCE_TEST_REDIRECT=1")
	if output, e := cmd.CombinedOutput(); e != nil {
		t.Fatalf("redirect: %v %s", e, output)
	}
	deadline := time.Now().Add(5 * time.Second)
	for !exists(filepath.Join(root, "redirect-started.json")) && time.Now().Before(deadline) {
		time.Sleep(20 * time.Millisecond)
	}
	var result struct {
		Directory string
		Args      []string
	}
	b, e := os.ReadFile(filepath.Join(root, "redirect-started.json"))
	if e != nil {
		t.Fatal(e)
	}
	_ = json.Unmarshal(b, &result)
	if result.Directory != root || len(result.Args) != 5 || result.Args[0] != "--launcher-redirect-parent" || result.Args[3] != "-window" || result.Args[4] != "argument é with spaces" {
		t.Fatalf("incorrect redirect: %+v", result)
	}
	// With the launcher's marker, the engine consumes it and reaches its normal
	// diagnostic entry point instead of redirecting again.
	_ = os.Remove(filepath.Join(root, "redirect-started.json"))
	cmd = exec.Command(filepath.Join(root, "arcanum-modern.exe"), "--renaissance-launcher", "--check-movies")
	cmd.Dir = root
	if output, e := cmd.CombinedOutput(); e != nil {
		t.Fatalf("marker: %v %s", e, output)
	}
	if exists(filepath.Join(root, "redirect-started.json")) {
		t.Fatal("launcher loop")
	}
}
func TestNativeLockedFileRollback(t *testing.T) {
	root := t.TempDir()
	old := fixture(t, root, "2.0.1")
	payload := t.TempDir()
	next := fixture(t, payload, "2.0.2")
	p, _ := syscall.UTF16PtrFromString(filepath.Join(root, "modern-ui/Book.ttf"))
	h, e := syscall.CreateFile(p, syscall.GENERIC_READ, syscall.FILE_SHARE_READ, nil, syscall.OPEN_EXISTING, 0, 0)
	if e != nil {
		t.Fatal(e)
	}
	e = install(root, payload, next, nil)
	syscall.CloseHandle(h)
	if e == nil {
		t.Fatal("expected locked file failure")
	}
	if e = recoverInstall(root); e != nil {
		t.Fatal(e)
	}
	for name, want := range old.Hashes {
		got, e := fileHash(filepath.Join(root, name))
		if e != nil || got != want {
			t.Fatalf("failed rollback: %s", name)
		}
	}
}
func TestNativeLauncherBootstrap(t *testing.T) {
	binary := os.Getenv("RENAISSANCE_LAUNCHER_BINARY")
	if binary == "" {
		t.Skip("set RENAISSANCE_LAUNCHER_BINARY for the native bootstrap smoke test")
	}
	root := t.TempDir()
	fixture(t, root, "9999.0.0") // Never update this synthetic fixture from a public release.
	source, e := os.ReadFile(binary)
	if e != nil {
		t.Fatal(e)
	}
	put(t, root, "renaissance-launcher.exe", source)
	exe, _ := os.Executable()
	game, e := os.ReadFile(exe)
	if e != nil {
		t.Fatal(e)
	}
	put(t, root, "arcanum-modern.exe", game)
	put(t, root, "arcanum1.dat", []byte("test"))
	put(t, root, "modern-ui/launcher.json", []byte(`{"language":"en","auto_play":true}`))
	cmd := exec.Command(filepath.Join(root, "renaissance-launcher.exe"), "-window", "-geometry=1280x720")
	cmd.Env = append(os.Environ(), "RENAISSANCE_TEST_GAME=1")
	if e = cmd.Run(); e != nil {
		t.Fatal(e)
	}
	// The initial launcher exits before the worker installs anything. Its file
	// must therefore be replaceable while the launcher's window is running.
	name := filepath.Join(root, "renaissance-launcher.exe")
	if e = os.Rename(name, name+".previous"); e != nil {
		t.Fatal("installed launcher remains locked:", e)
	}
	put(t, root, "renaissance-launcher.exe", source)
	deadline := time.Now().Add(25 * time.Second)
	for !exists(filepath.Join(root, "game-started.json")) && time.Now().Before(deadline) {
		time.Sleep(50 * time.Millisecond)
	}
	if !exists(filepath.Join(root, "game-started.json")) {
		t.Fatal("worker did not start the fixture game")
	}
	for gameRunning(root) && time.Now().Before(deadline) {
		time.Sleep(50 * time.Millisecond)
	}
}
