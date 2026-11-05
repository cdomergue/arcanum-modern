package main

import (
	"crypto/sha256"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"unsafe"
)

var kernel = syscall.NewLazyDLL("kernel32.dll")
var user = syscall.NewLazyDLL("user32.dll")

func wide(s string) *uint16 {
	p, _ := syscall.UTF16PtrFromString(strings.ReplaceAll(s, "\x00", ""))
	return p
}

// Win32 receives integer addresses. Retain the small set of UI strings for
// the window lifetime so a GC during a callback cannot reclaim their UTF-16.
var winStrings sync.Map

func ptr(s string) uintptr {
	p, _ := winStrings.LoadOrStore(s, wide(s))
	return uintptr(unsafe.Pointer(p.(*uint16)))
}
func message(text string) {
	user.NewProc("MessageBoxW").Call(0, ptr(text), ptr("Arcanum Renaissance"), 0x10)
}
func systemLanguage() string {
	n, _, _ := kernel.NewProc("GetUserDefaultUILanguage").Call()
	if n&0x3ff == 0x0c {
		return "fr"
	}
	return "en"
}
func instance(root string) (func(), error) {
	hash := sha256.Sum256([]byte(strings.ToLower(root)))
	h, _, err := kernel.NewProc("CreateMutexW").Call(0, 0, ptr(fmt.Sprintf("Local\\ArcanumRenaissanceLauncher-%x", hash[:16])))
	if h == 0 {
		return nil, err
	}
	if err == syscall.Errno(183) {
		syscall.CloseHandle(syscall.Handle(h))
		return nil, fmt.Errorf("Arcanum Renaissance is already running. / Arcanum Renaissance est déjà lancé.")
	}
	return func() { syscall.CloseHandle(syscall.Handle(h)) }, nil
}
func gameRunning(root string) bool {
	// Query executable paths: another Arcanum installation must not block this one.
	snapshot, _, _ := kernel.NewProc("CreateToolhelp32Snapshot").Call(2, 0)
	if snapshot == ^uintptr(0) {
		return true
	}
	defer syscall.CloseHandle(syscall.Handle(snapshot))
	type entry struct {
		Size, Usage, PID        uint32
		Heap                    uintptr
		Module, Threads, Parent uint32
		Priority                int32
		Flags                   uint32
		Exe                     [260]uint16
	}
	pe := entry{}
	pe.Size = uint32(unsafe.Sizeof(pe))
	ok, _, _ := kernel.NewProc("Process32FirstW").Call(snapshot, uintptr(unsafe.Pointer(&pe)))
	target := filepath.Join(root, "arcanum-modern.exe")
	for ok != 0 {
		if strings.EqualFold(syscall.UTF16ToString(pe.Exe[:]), "arcanum-modern.exe") {
			h, _, _ := kernel.NewProc("OpenProcess").Call(0x1000, 0, uintptr(pe.PID))
			if h == 0 {
				return true
			}
			buffer := make([]uint16, 32768)
			size := uint32(len(buffer))
			got, _, _ := kernel.NewProc("QueryFullProcessImageNameW").Call(h, 0, uintptr(unsafe.Pointer(&buffer[0])), uintptr(unsafe.Pointer(&size)))
			syscall.CloseHandle(syscall.Handle(h))
			if got == 0 || strings.EqualFold(syscall.UTF16ToString(buffer[:size]), target) {
				return true
			}
		}
		ok, _, _ = kernel.NewProc("Process32NextW").Call(snapshot, uintptr(unsafe.Pointer(&pe)))
	}
	return false
}
func bootstrap(executable, root string, args []string) error {
	release, err := instance(root)
	if err != nil {
		return err
	}
	defer release()
	if gameRunning(root) {
		return fmt.Errorf("Close Arcanum Renaissance first. / Fermez d’abord Arcanum Renaissance.")
	}
	cache, err := os.UserCacheDir()
	if err != nil {
		return err
	}
	dir := filepath.Join(cache, "ArcanumRenaissance", "launcher")
	if err = os.MkdirAll(dir, 0700); err != nil {
		return err
	}
	hash := sha256.Sum256([]byte(strings.ToLower(root)))
	worker := filepath.Join(dir, fmt.Sprintf("%x.exe", hash[:16]))
	// Execute a private copy so the installed launcher can be replaced with the
	// rest of the package. One cache file per installation is reused next launch.
	in, err := os.Open(executable)
	if err != nil {
		return err
	}
	defer in.Close()
	out, err := os.CreateTemp(dir, "worker-*.tmp")
	if err != nil {
		return err
	}
	temporary := out.Name()
	defer os.Remove(temporary)
	_, err = io.Copy(out, in)
	closeErr := out.Close()
	if err != nil {
		return err
	}
	if closeErr != nil {
		return closeErr
	}
	if err = os.Rename(temporary, worker); err != nil {
		return err
	}
	command := exec.Command(worker, append([]string{"--launcher-worker", root, strconv.Itoa(os.Getpid()), "--"}, args...)...)
	command.Dir = root
	return command.Start()
}
func main() {
	exe, err := os.Executable()
	if err != nil {
		message(err.Error())
		return
	}
	root := filepath.Dir(exe)
	args := os.Args[1:]
	if len(args) >= 3 && args[0] == "--launcher-redirect-parent" && args[2] == "--" {
		if err := waitForParent(args[1]); err != nil {
			message(err.Error())
			return
		}
		args = args[3:]
	}
	preview := false
	lang := ""
	if len(args) > 0 && args[0] == "--launcher-preview" {
		preview = true
		if len(args) > 1 {
			lang = args[1]
		}
		args = nil
	} else if len(args) >= 4 && args[0] == "--launcher-worker" && args[3] == "--" {
		root = args[1]
		if err := waitForParent(args[2]); err != nil {
			message(err.Error())
			return
		}
		args = args[4:]
	} else {
		root, err = filepath.EvalSymlinks(root)
		if err == nil {
			err = bootstrap(exe, root, args)
		}
		if err != nil {
			message(err.Error())
		}
		return
	}
	release, err := instance(root)
	if err != nil {
		message(err.Error())
		return
	}
	defer release()
	if !preview {
		if !exists(filepath.Join(root, "arcanum1.dat")) || (!exists(filepath.Join(root, "arcanum-modern.exe")) && !exists(filepath.Join(root, transactionDir))) {
			message("Place the complete Renaissance package in your Arcanum game folder.\nPlacez Renaissance dans le dossier de votre jeu Arcanum.")
			return
		}
	}
	p := loadPreferences(root)
	if lang == "en" || lang == "fr" {
		p.Language = lang
	}
	if preview {
		p.AutoPlay = false
	}
	a := newApplication(root, args, preview)
	if err = showLauncher(a, p); err != nil {
		message(err.Error())
		a.stop()
		<-a.done
	}
}

func waitForParent(value string) error {
	pid, err := strconv.ParseUint(value, 10, 32)
	if err != nil || pid == 0 {
		return fmt.Errorf("invalid launcher parent")
	}
	h, _, _ := kernel.NewProc("OpenProcess").Call(0x100000, 0, uintptr(pid))
	if h == 0 {
		return nil
	} // Parent has already exited.
	defer syscall.CloseHandle(syscall.Handle(h))
	result, _, _ := kernel.NewProc("WaitForSingleObject").Call(h, 15000)
	if result != 0 {
		return fmt.Errorf("previous game process is still running")
	}
	return nil
}

func runGame(root string, args []string) error {
	cmd := exec.Command(filepath.Join(root, gameRelative()), args...)
	cmd.Dir = root
	for _, arg := range args {
		if arg == "-window" {
			cmd.Env = append(os.Environ(), "ARCANUM_IGNORE_DISPLAY_SETTINGS=1")
			break
		}
	}
	return cmd.Run()
}
