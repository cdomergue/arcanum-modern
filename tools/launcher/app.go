package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"

	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"
)

type view struct {
	Phase                 string
	Percent               int
	Tag, Notes, Installed string
	Ready, Fatal          bool
}
type application struct {
	mu      sync.Mutex
	state   view
	root    string
	args    []string
	cancel  context.CancelFunc
	done    chan struct{}
	preview bool
}

func (a *application) snapshot() view    { a.mu.Lock(); defer a.mu.Unlock(); return a.state }
func (a *application) set(f func(*view)) { a.mu.Lock(); defer a.mu.Unlock(); f(&a.state) }
func (a *application) phase(p string)    { a.set(func(v *view) { v.Phase = p }) }
func (a *application) record(err error) {
	name := filepath.Join(a.root, "ArcanumModern", "launcher.log")
	if f, e := os.OpenFile(name, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600); e == nil {
		fmt.Fprintf(f, "%s %v\n", time.Now().Format(time.RFC3339), err)
		f.Close()
	}
}
func (a *application) finish(phase string, fatal bool) {
	a.set(func(v *view) { v.Phase = phase; v.Ready = true; v.Fatal = fatal })
}
func (a *application) run(ctx context.Context) {
	defer close(a.done)
	if err := recoverInstall(a.root); err != nil {
		a.record(err)
		a.finish("recovery", true)
		return
	}
	if m, e := readManifest(a.root); e == nil {
		if _, valid := version(m.Version); valid {
			a.set(func(v *view) { v.Installed = m.Version })
		}
	}
	client := updateClient()
	r, err := latest(ctx, client, releaseAPI)
	if err != nil {
		a.record(err)
		if ctx.Err() != nil {
			a.finish("skipped", false)
		} else {
			a.finish("offline", false)
		}
		return
	}
	a.set(func(v *view) { v.Tag = r.Tag; v.Notes = releaseText(r.Body) })
	// Cache only release text; it is displayed as text and never executed.
	if raw, e := json.Marshal(r); e == nil {
		_ = os.WriteFile(filepath.Join(a.root, "ArcanumModern", "launcher-release.json"), raw, 0600)
	}
	if !newer(a.snapshot().Installed, r.Tag) {
		a.finish("current", false)
		return
	}
	if gameRunning(a.root) {
		a.finish("running", true)
		return
	}
	work, err := downloadWorkspace(a.root)
	if err != nil {
		a.record(err)
		a.finish("failed", false)
		return
	}
	defer os.RemoveAll(work)
	a.phase("download")
	err = download(ctx, client, r, filepath.Join(work, "update.zip"), func(p int) { a.set(func(v *view) { v.Percent = p }) })
	if err == nil && ctx.Err() == nil {
		a.phase("verify")
		var m manifest
		payload := filepath.Join(work, "payload")
		m, err = unpack(filepath.Join(work, "update.zip"), payload, r.Tag)
		if err == nil && ctx.Err() == nil {
			if gameRunning(a.root) {
				a.finish("running", true)
				return
			}
			// UI cancels only before this phase; from here it waits for commit/rollback.
			a.phase("install")
			err = install(a.root, payload, m, nil)
			if err == nil {
				a.set(func(v *view) { v.Installed = m.Version; v.Percent = 100 })
				a.finish("updated", false)
				return
			}
		}
	}
	if ctx.Err() != nil {
		a.finish("skipped", false)
		return
	}
	a.record(err)
	if e := recoverInstall(a.root); e != nil {
		a.record(e)
		a.finish("recovery", true)
		return
	}
	a.finish("failed", false)
}
func newApplication(root string, args []string, preview bool) *application {
	a := &application{root: root, args: args, done: make(chan struct{}), preview: preview, state: view{Phase: "check", Installed: buildVersion}}
	if m, e := readManifest(root); e == nil {
		if _, valid := version(m.Version); valid {
			a.state.Installed = m.Version
		}
	}
	if b, e := os.ReadFile(filepath.Join(root, "ArcanumModern", "launcher-release.json")); e == nil {
		var r release
		if json.Unmarshal(b, &r) == nil {
			a.state.Tag = r.Tag
			a.state.Notes = releaseText(r.Body)
		}
	}
	ctx, cancel := context.WithCancel(context.Background())
	a.cancel = cancel
	if preview {
		a.state = view{Phase: "download", Percent: 64, Installed: buildVersion, Tag: "v2.0.2", Notes: releaseText("## Arcanum Renaissance\n\n### Character creation\n- Import a custom portrait, crop it and convert it in game.\n- Create backgrounds with native effects and advanced editing.\n\n### Improvements\n- Various bug fixes not covered by UAP.\n\nRelease notes are displayed in their published language.\n\n### Compatibility\nYour saves, settings and custom content are preserved.\n\n### Installation\nBoth the setup and the ZIP include the Renaissance launcher.\n\n### Credits\nBased on Arcanum Community Edition.")}
		close(a.done)
	} else {
		go a.run(ctx)
	}
	return a
}

var markdownLink = regexp.MustCompile(`\[([^\]]+)\]\(([^)]+)\)`)

func releaseText(raw string) string {
	raw = strings.ReplaceAll(raw, "\r", "")
	raw = markdownLink.ReplaceAllString(raw, "$1 ($2)")
	lines := strings.Split(raw, "\n")
	for i, line := range lines {
		line = strings.TrimLeft(line, "#")
		line = strings.TrimSpace(line)
		line = strings.ReplaceAll(line, "**", "")
		line = strings.ReplaceAll(line, "`", "")
		if strings.HasPrefix(line, "- ") {
			line = "• " + line[2:]
		}
		lines[i] = line
	}
	return strings.Join(lines, "\r\n")
}
func (a *application) launch() error {
	if a.preview {
		return nil
	}
	if gameRunning(a.root) {
		return fmt.Errorf("Arcanum Renaissance is already running")
	}
	args := a.args
	if len(args) == 0 {
		args = []string{"-geometry=1280x720"}
	}
	args = append([]string{"--renaissance-launcher"}, args...)
	return runGame(a.root, args)
}

type preferences struct {
	Language string `json:"language"`
	AutoPlay bool   `json:"auto_play"`
}

func loadPreferences(root string) preferences {
	p := preferences{Language: systemLanguage(), AutoPlay: true}
	if b, e := os.ReadFile(filepath.Join(root, "modern-ui", "launcher.json")); e == nil {
		_ = json.Unmarshal(b, &p)
	}
	if p.Language != "fr" {
		p.Language = "en"
	}
	return p
}
func savePreferences(root string, p preferences) {
	b, e := json.Marshal(p)
	if e == nil {
		_ = os.WriteFile(filepath.Join(root, "modern-ui", "launcher.json"), b, 0600)
	}
}
