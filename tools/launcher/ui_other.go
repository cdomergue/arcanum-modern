//go:build !windows

package main

import (
	"bufio"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"
)

// SDL provides a native window on Linux and macOS, with no browser or GTK.
func showLauncher(a *application, p preferences) error {
	work, e := os.MkdirTemp("", "renaissance-ui-")
	if e != nil {
		return e
	}
	defer os.RemoveAll(work)
	statePath := filepath.Join(work, "state.txt")
	command := exec.Command(filepath.Join(a.root, "ArcanumModern/launcher/renaissance-launcher-ui"), statePath, filepath.Join(a.root, "ArcanumModern/launcher/background.png"), filepath.Join(a.root, "modern-ui/Book.ttf"))
	command.Dir = a.root
	command.Stderr = os.Stderr
	output, e := command.StdoutPipe()
	if e != nil {
		return e
	}
	events := make(chan string, 16)
	exited := make(chan error, 1)
	writeState := func(v view, seconds int) error {
		tr := func(fr, en string) string { return localized(p.Language, fr, en) }
		play := tr("Jouer", "Play")
		if !v.Ready {
			play = tr("Jouer sans mise à jour", "Play without updating")
		}
		notes := v.Notes
		if notes == "" {
			notes = tr("Les notes de la dernière release s’afficheront ici.", "The latest release notes will appear here.")
		}
		status := statusText(v, p.Language)
		if v.Ready && p.AutoPlay && !v.Fatal {
			status += fmt.Sprintf(" (%d s)", seconds)
		}
		text := fmt.Sprintf("%d %t %t %t\n%s\n%s\n%s\n%s\n%s\n%s", v.Percent, p.AutoPlay, v.Phase != "install" && !v.Fatal, v.Phase != "install", status, tr("Version installée : ", "Installed version: ")+v.Installed, tr("NOTES DE VERSION", "RELEASE NOTES")+" · "+v.Tag, play, tr("Lancer le jeu automatiquement", "Launch the game automatically"), strings.ReplaceAll(notes, "\r", ""))
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
	for {
		select {
		case e := <-exited:
			a.cancel()
			<-a.done
			return e
		case action := <-events:
			v := a.snapshot()
			switch action {
			case "play":
				if v.Phase != "install" && !v.Fatal {
					play = true
					a.cancel()
				}
			case "quit":
				if v.Phase != "install" {
					closing = true
					a.cancel()
				}
			case "auto":
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
		case <-tick.C:
		}
		v := a.snapshot()
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
		if v.Ready && !v.Fatal && (play || (p.AutoPlay && time.Since(ready) >= 4*time.Second)) {
			stop()
			return a.launch()
		}
	}
}
