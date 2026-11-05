//go:build !windows

package main

import (
	"crypto/sha256"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"syscall"
	"time"
)

func systemLanguage() string {
	if strings.HasPrefix(strings.ToLower(os.Getenv("LANG")), "fr") {
		return "fr"
	}
	return "en"
}
func instance(root string) (func(), error) {
	cache, e := os.UserCacheDir()
	if e != nil {
		return nil, e
	}
	dir := filepath.Join(cache, "ArcanumRenaissance")
	if e = os.MkdirAll(dir, 0700); e != nil {
		return nil, e
	}
	hash := sha256.Sum256([]byte(root))
	f, e := os.OpenFile(filepath.Join(dir, fmt.Sprintf("launcher-%x.lock", hash[:16])), os.O_CREATE|os.O_RDWR, 0600)
	if e != nil {
		return nil, e
	}
	if e = syscall.Flock(int(f.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); e != nil {
		f.Close()
		return nil, fmt.Errorf("Arcanum Renaissance is already running")
	}
	return func() { f.Close() }, nil
}
func gameRunning(root string) bool {
	target := filepath.Join(root, gameRelative())
	if runtime.GOOS == "linux" {
		entries, _ := os.ReadDir("/proc")
		for _, entry := range entries {
			if _, e := strconv.Atoi(entry.Name()); e != nil {
				continue
			}
			exe, e := os.Readlink(filepath.Join("/proc", entry.Name(), "exe"))
			if e == nil && strings.TrimSuffix(exe, " (deleted)") == target {
				return true
			}
		}
		return false
	}
	out, e := exec.Command("/bin/ps", "-axo", "comm=").Output()
	if e != nil {
		return true
	}
	for _, line := range strings.Split(string(out), "\n") {
		if strings.TrimSpace(line) == target {
			return true
		}
	}
	return false
}
func runGame(root string, args []string) error {
	env := os.Environ()
	for _, arg := range args {
		if arg == "-window" {
			env = append(env, "ARCANUM_IGNORE_DISPLAY_SETTINGS=1")
			break
		}
	}
	auto := runtime.GOOS == "linux" && os.Getenv("SDL_RENDER_DRIVER") == ""
	if auto {
		env = append(env, "SDL_RENDER_DRIVER=vulkan")
	}
	if runtime.GOOS == "linux" && os.Getenv("SDL_RENDER_DRIVER") == "software" {
		if os.Getenv("SDL_VIDEO_DRIVER") == "" {
			env = append(env, "SDL_VIDEO_DRIVER=x11")
		}
		if os.Getenv("SDL_FRAMEBUFFER_ACCELERATION") == "" {
			env = append(env, "SDL_FRAMEBUFFER_ACCELERATION=0")
		}
	}
	run := func(environment []string) error {
		log, e := os.Create(filepath.Join(root, "modern-launch.log"))
		if e != nil {
			return e
		}
		defer log.Close()
		cmd := exec.Command(filepath.Join(root, gameRelative()), args...)
		cmd.Dir = root
		cmd.Env = environment
		cmd.Stdout = log
		cmd.Stderr = log
		return cmd.Run()
	}
	err := run(env)
	if auto && err != nil {
		if status, ok := err.(*exec.ExitError); ok && (status.ExitCode() == 139 || status.Sys().(syscall.WaitStatus).Signal() == syscall.SIGSEGV) {
			log, _ := os.ReadFile(filepath.Join(root, "modern-launch.log"))
			if !strings.Contains(string(log), "Arcanum Modern: startup complete") {
				_ = os.WriteFile(filepath.Join(root, "modern-launch-vulkan.log"), log, 0600)
				env = append(env, "SDL_RENDER_DRIVER=software", "SDL_VIDEO_DRIVER=x11", "SDL_FRAMEBUFFER_ACCELERATION=0")
				return run(env)
			}
		}
	}
	return err
}
func main() {
	exe, e := os.Executable()
	if e != nil {
		fmt.Fprintln(os.Stderr, e)
		return
	}
	exe, e = filepath.EvalSymlinks(exe)
	if e != nil {
		fmt.Fprintln(os.Stderr, e)
		return
	}
	root := filepath.Dir(exe)
	if runtime.GOOS == "darwin" && filepath.Base(root) == "MacOS" {
		root = filepath.Dir(filepath.Dir(filepath.Dir(root)))
	}
	args := os.Args[1:]
	preview := false
	if len(args) >= 3 && args[0] == "--launcher-redirect-parent" && args[2] == "--" {
		pid, e := strconv.Atoi(args[1])
		if e != nil || pid < 1 {
			return
		}
		deadline := time.Now().Add(15 * time.Second)
		for syscall.Kill(pid, 0) == nil && time.Now().Before(deadline) {
			time.Sleep(20 * time.Millisecond)
		}
		args = args[3:]
	}
	if len(args) > 0 && args[0] == "--launcher-preview" {
		preview = true
		args = nil
	}
	release, e := instance(root)
	if e != nil {
		fmt.Fprintln(os.Stderr, e)
		return
	}
	defer release()
	if !preview && (!exists(filepath.Join(root, "arcanum1.dat")) || (!exists(filepath.Join(root, gameRelative())) && !exists(filepath.Join(root, transactionDir)))) {
		fmt.Fprintln(os.Stderr, "Extract the complete Renaissance package beside arcanum1.dat.")
		return
	}
	if gameRunning(root) {
		fmt.Fprintln(os.Stderr, "Close Arcanum Renaissance first.")
		return
	}
	p := loadPreferences(root)
	if preview {
		p.AutoPlay = false
	}
	a := newApplication(root, args, preview)
	if e = showLauncher(a, p); e != nil {
		fmt.Fprintln(os.Stderr, e)
		a.cancel()
		<-a.done
	}
}
