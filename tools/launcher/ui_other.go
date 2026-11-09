//go:build !windows

package main

import (
	"bufio"
	"encoding/hex"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"syscall"
	"time"
)

// SDL provides a native window on Linux and macOS, with no browser or GTK.
func showLauncher(a *application, p preferences) error {
	log := newProcessLog(a.root, "launcher-ui.log")
	defer log.Close()
	defer func() { a.stop(); <-a.done }()
	start := time.Now()
	log.event("start", fmt.Sprintf("ui=true video=%q renderer=%q", os.Getenv("SDL_VIDEO_DRIVER"), os.Getenv("SDL_RENDER_DRIVER")))
	software := os.Getenv("RENAISSANCE_LAUNCHER_SOFTWARE") == "1"
	e := showLauncherAttempt(a, p, software, log)
	if !software && runtime.GOOS == "linux" && os.Getenv("SDL_RENDER_DRIVER") == "" && rendererCrashed(e) {
		log.event("fallback", fmt.Sprintf("elapsed_ms=%d error=%v renderer=software video=x11", time.Since(start).Milliseconds(), e))
		e = showLauncherAttempt(a, p, true, log)
	}
	log.event("exit", fmt.Sprintf("elapsed_ms=%d error=%v", time.Since(start).Milliseconds(), e))
	return e
}

func rendererCrashed(err error) bool {
	e, ok := err.(*exec.ExitError)
	if !ok {
		return false
	}
	status, ok := e.Sys().(syscall.WaitStatus)
	return ok && status.Signaled() && (status.Signal() == syscall.SIGSEGV || status.Signal() == syscall.SIGABRT)
}

func showLauncherAttempt(a *application, p preferences, software bool, log io.Writer) error {
	work, e := os.MkdirTemp("", "renaissance-ui-")
	if e != nil {
		return e
	}
	defer os.RemoveAll(work)
	statePath := filepath.Join(work, "state.txt")
	command := exec.Command(filepath.Join(a.root, "ArcanumModern/launcher/renaissance-launcher-ui"), statePath, filepath.Join(a.root, "ArcanumModern/launcher/background.png"), filepath.Join(a.root, "modern-ui/Book.ttf"))
	command.Dir = a.root
	command.Stderr = io.MultiWriter(os.Stderr, log)
	if software {
		command.Env = append(os.Environ(), "SDL_RENDER_DRIVER=software", "SDL_VIDEO_DRIVER=x11", "SDL_FRAMEBUFFER_ACCELERATION=0")
	}
	output, e := command.StdoutPipe()
	if e != nil {
		return e
	}
	events := make(chan string, 16)
	exited := make(chan error, 1)
	communityTab, communityBusy := false, false
	communityStatus := ""
	communityResult := make(chan string, 1)
	writeState := func(v view, seconds int) error {
		tr := func(fr, en string) string { return localized(p.Language, fr, en) }
		play := v.playLabel(p.Language)
		notes := v.notes(p.Language)
		status := statusText(v, p.Language)
		if !communityTab && v.autoStart() && p.AutoPlay {
			status += fmt.Sprintf(" (%d s)", seconds)
		}
		text := fmt.Sprintf("%d %t %t %t %d\n%s\n%s\n%s\n%s\n%s\n%s", v.Percent, p.AutoPlay, !v.busy() && !v.Fatal, !v.busy(), v.Offer, status, tr("Version installée : ", "Installed version: ")+v.Installed, v.heading(p.Language), play, v.secondaryLabel(p.Language), strings.ReplaceAll(notes, "\r", ""))
		if communityTab {
			statusText := communityStatus
			if statusText == "" {
				statusText = tr("10–2000 caractères. Message et version envoyés au développeur.", "10–2000 characters. Message and version sent to the developer.")
			}
			if communityURL == "" {
				statusText = tr("Service non configuré : aucun envoi ni collecte.", "Service not configured: no sending or collection.")
			}
			text = fmt.Sprintf("%d %t %t %t %d\n%s\n%s\n%s\n%s\n%s\n%s", v.Percent, p.AutoPlay, !v.busy() && !v.Fatal, !v.busy(), v.Offer, status, tr("Version installée : ", "Installed version: ")+v.Installed, "Feedbacks", play, v.secondaryLabel(p.Language), statusText)
		}
		flag := func(b bool) int {
			if b {
				return 1
			}
			return 0
		}
		text = strings.Replace(text, "\n", fmt.Sprintf(" %d %d %d %d\n", flag(communityTab), flag(communityURL != "" && !a.preview), flag(communityBusy), flag(p.Language == "fr")), 1)
		if e := os.WriteFile(statePath+".privacy", []byte(communityNotice(p.Language)), 0600); e != nil {
			return e
		}
		if e := os.WriteFile(statePath+".new", []byte(text), 0600); e != nil {
			return e
		}
		return os.Rename(statePath+".new", statePath)
	}
	if e = writeState(a.snapshot(), 4); e != nil {
		return e
	}
	if e = command.Start(); e != nil {
		return e
	}
	go func() {
		scanner := bufio.NewScanner(output)
		for scanner.Scan() {
			events <- scanner.Text()
		}
		exited <- command.Wait()
	}()
	stop := func() {
		_ = command.Process.Signal(os.Interrupt)
		select {
		case <-exited:
		case <-time.After(2 * time.Second):
			_ = command.Process.Kill()
			<-exited
		}
	}
	tick := time.NewTicker(100 * time.Millisecond)
	defer tick.Stop()
	ready := time.Time{}
	play, closing := false, false
	last := ""
	lastPhase := ""
	for {
		select {
		case status := <-communityResult:
			communityBusy = false
			communityStatus = status
			last = ""
		case e := <-exited:
			return e
		case action := <-events:
			v := a.snapshot()
			if strings.HasPrefix(action, "feedback:") && !communityBusy && !a.preview && communityURL != "" {
				body, err := hex.DecodeString(strings.TrimPrefix(action, "feedback:"))
				if err == nil {
					communityBusy = true
					communityStatus = localized(p.Language, "Envoi…", "Sending…")
					language := p.Language
					go func() {
						status := localized(language, "Merci, votre retour a été envoyé.", "Thank you, your feedback was sent.")
						if sendFeedback(string(body)) != nil {
							status = localized(language, "Envoi impossible. Vérifiez la connexion et le texte (10–2000 caractères).", "Could not send. Check connection and text (10–2000 characters).")
						}
						communityResult <- status
					}()
				}
			}
			switch action {
			case "github":
				opener := "xdg-open"
				if runtime.GOOS == "darwin" {
					opener = "open"
				}
				go func() { _ = exec.Command(opener, "https://github.com/cdomergue/arcanum-modern").Run() }()
			case "news", "feedback":
				communityTab = action == "feedback"
				ready = time.Now()
				play = false
			case "play":
				if v.Offer != 0 {
					play = false
					a.respond(false)
				} else if !v.busy() && !v.Fatal {
					play = true
					a.cancel()
				}
			case "quit":
				if !v.busy() {
					closing = true
					a.stop()
				}
			case "auto":
				if v.Offer != 0 {
					play = false
					a.respond(true)
					break
				}
				p.AutoPlay = !p.AutoPlay
				ready = time.Now()
			case "language":
				if p.Language == "fr" {
					p.Language = "en"
				} else {
					p.Language = "fr"
				}
			}
			if !a.preview {
				savePreferences(a.root, p)
			}
			last = ""
		case <-tick.C:
		}
		v := a.snapshot()
		if v.Phase == "uap-failed" && lastPhase != "uap-failed" {
			play = false
		}
		lastPhase = v.Phase
		if v.Offer != 0 {
			play = false
		}
		if !v.Ready {
			ready = time.Time{}
		}
		if v.Ready && ready.IsZero() {
			ready = time.Now()
		}
		seconds := 4
		if !ready.IsZero() {
			seconds -= int(time.Since(ready).Seconds())
		}
		if seconds < 0 {
			seconds = 0
		}
		fingerprint := fmt.Sprintf("%+v/%+v/%d", v, p, seconds)
		if fingerprint != last {
			if e = writeState(v, seconds); e != nil {
				stop()
				return e
			}
			last = fingerprint
		}
		if closing && (v.Ready || a.preview) {
			stop()
			return nil
		}
		if v.Ready && !v.Fatal && !communityBusy && (play || (!communityTab && v.autoStart() && p.AutoPlay && time.Since(ready) >= 4*time.Second)) {
			stop()
			return a.launch()
		}
	}
}
