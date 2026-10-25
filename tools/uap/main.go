package main

import (
	"bytes"
	"compress/gzip"
	"embed"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
)

//go:embed data/catalog.json data/plan.json.gz
var assets embed.FS

type catalog struct {
	Original map[string]backupEntry `json:"original"`
	Patch    map[string]backupEntry `json:"patch"`
}

var work string

func progress(phase string, value int) {
	fmt.Printf("%s %d\n", phase, value)
	if work != "" {
		_ = os.WriteFile(filepath.Join(work, "progress.new"), []byte(fmt.Sprintf("%s %d\n", phase, value)), 0600)
		_ = os.Rename(filepath.Join(work, "progress.new"), filepath.Join(work, "progress"))
	}
}
func loadCatalog() (catalog, error) {
	var c catalog
	raw, err := assets.ReadFile("data/catalog.json")
	if err == nil {
		err = json.Unmarshal(raw, &c)
	}
	return c, err
}
func verify(path string, want backupEntry) error {
	st, err := os.Stat(path)
	if err != nil {
		return err
	}
	if !st.Mode().IsRegular() || st.Size() != want.Size {
		return fmt.Errorf("unexpected file size: %s", path)
	}
	sum, _, err := fileHash(path)
	if err != nil {
		return err
	}
	if sum != want.SHA256 {
		return fmt.Errorf("unexpected checksum: %s", path)
	}
	return nil
}
func compatibleLayout(game string, c catalog) error {
	entries, err := os.ReadDir(game)
	if err != nil {
		return err
	}
	for _, e := range entries {
		n := strings.ToLower(e.Name())
		if strings.HasPrefix(n, "arcanum") && strings.HasSuffix(n, ".dat") {
			if _, ok := c.Original[n]; !ok {
				return fmt.Errorf("additional game archive: %s", n)
			}
		}
	}
	modules, err := findCase(game, "modules")
	if err != nil {
		return err
	}
	entries, err = os.ReadDir(modules)
	if err != nil {
		return err
	}
	for _, e := range entries {
		n := strings.ToLower(e.Name())
		if strings.Contains(n, ".patch") && n != "arcanum.patch0" {
			a, e := openDAT(filepath.Join(modules, e.Name()))
			if e != nil {
				return e
			}
			mapsOnly := strings.HasSuffix(n, ".patch9")
			for resource := range a.entries {
				mapsOnly = mapsOnly && strings.HasPrefix(resource, "townmap/")
			}
			if !mapsOnly {
				return fmt.Errorf("additional module patch: %s", n)
			}
		}
	}
	return nil
}
func compatible(game string, c catalog) error {
	if err := compatibleLayout(game, c); err != nil {
		return err
	}
	for name, want := range c.Original {
		p, e := findCase(game, name)
		if e != nil {
			return e
		}
		if e = verify(p, want); e != nil {
			return e
		}
	}
	return nil
}

// English releases can package localized archives differently. Verify the
// shared art archives, parse every required DAT and identify the installed
// language from the game's own interface text. Additional mods are excluded.
func compatibleEnglish(game string, c catalog) error {
	if err := compatibleLayout(game, c); err != nil {
		return err
	}
	for name, want := range c.Original {
		p, err := findCase(game, name)
		if err != nil {
			return err
		}
		if name == "arcanum1.dat" || name == "arcanum2.dat" {
			if err = verify(p, want); err != nil {
				return err
			}
		}
		a, err := openDAT(p)
		if err != nil {
			return err
		}
		if len(a.entries) == 0 {
			return fmt.Errorf("empty game archive: %s", name)
		}
	}
	repos, err := archiveSet(game)
	if err != nil {
		return err
	}
	raw, err := readResource(repos, "mes/intgame.mes")
	if err != nil {
		return err
	}
	// Loose text takes precedence in the engine too.
	if p, e := findCase(game, "data/mes/intgame.mes"); e == nil {
		raw, err = os.ReadFile(p)
		if err != nil {
			return err
		}
	} else if !os.IsNotExist(e) {
		return e
	}
	for _, row := range parseRecords(raw, 2) {
		if row.ID == 0 && strings.TrimSpace(string(row.Fields[1])) == "Level" {
			return nil
		}
	}
	return fmt.Errorf("unrecognized English game text")
}

func detectEdition(game string, c catalog) (string, error) {
	if err := compatible(game, c); err == nil {
		return "fr", nil
	}
	if err := compatibleEnglish(game, c); err != nil {
		return "", err
	}
	return "en", nil
}

func overlayVersion(language string) string { return "2.0.2-" + language + "-1" }
func overlayReady(root, language string) bool {
	b, err := os.ReadFile(filepath.Join(root, "modern-ui", "uap-"+language, "active", "ready"))
	return err == nil && string(b) == overlayVersion(language)+"\n"
}

func copyVerified(source, target string, want backupEntry) error {
	if err := verify(source, want); err != nil {
		return err
	}
	if err := os.MkdirAll(filepath.Dir(target), 0700); err != nil {
		return err
	}
	input, err := os.Open(source)
	if err != nil {
		return err
	}
	defer input.Close()
	output, err := os.OpenFile(target, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return err
	}
	_, err = io.Copy(output, input)
	if e := output.Sync(); err == nil {
		err = e
	}
	if e := output.Close(); err == nil {
		err = e
	}
	if err != nil {
		return err
	}
	return verify(target, want)
}
func prepare(game, installer, seven string, c catalog, language string) error {
	edition, err := detectEdition(game, c)
	if err != nil {
		return err
	}
	if edition != language {
		return fmt.Errorf("game language changed: %s", edition)
	}
	active := filepath.Join(work, "active")
	if _, err := os.Stat(active); err == nil {
		return fmt.Errorf("an active UAP overlay already exists")
	}
	stage, err := os.MkdirTemp(work, "stage-")
	if err != nil {
		return err
	}
	defer os.RemoveAll(stage)
	if installer == "" {
		installer = filepath.Join(stage, "UAP2.0.2.exe")
		progress("download", 0)
		if err = downloadInstaller(installer); err != nil {
			return err
		}
	}
	progress("verify", 0)
	if err = verify(installer, backupEntry{installerSize, installerSHA}); err != nil {
		return err
	}
	unpack := filepath.Join(stage, "unpacked")
	progress("extract", 0)
	args := []string{"x", "-y", "-o" + unpack, installer}
	keys := make([]string, 0, len(c.Patch))
	for name := range c.Patch {
		keys = append(keys, name)
	}
	sort.Strings(keys)
	// NSIS paths retain their original capitalization; extract into the private stage.
	command := exec.Command(seven, args...)
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr
	if err = command.Run(); err != nil {
		return fmt.Errorf("UAP extraction failed: %w", err)
	}
	overlay := filepath.Join(stage, "overlay")
	if err = os.Mkdir(overlay, 0700); err != nil {
		return err
	}
	for i, name := range keys {
		source, e := findCase(unpack, name)
		if e != nil {
			return e
		}
		target := name
		if !strings.Contains(name, "/") {
			target = "global/" + name
		}
		if e = copyVerified(source, filepath.Join(overlay, filepath.FromSlash(target)), c.Patch[name]); e != nil {
			return e
		}
		progress("files", (i+1)*100/len(keys))
	}
	if language == "fr" {
		progress("translate", 0)
		packed, _ := assets.ReadFile("data/plan.json.gz")
		reader, err := gzip.NewReader(bytes.NewReader(packed))
		if err != nil {
			return err
		}
		plan, err := io.ReadAll(reader)
		reader.Close()
		if err != nil {
			return err
		}
		planPath := filepath.Join(stage, "plan.json")
		if err = os.WriteFile(planPath, plan, 0600); err != nil {
			return err
		}
		if err = buildFrench(game, unpack, overlay, planPath); err != nil {
			return err
		}
	}
	progress("backup", 0)
	backup, err := backupSaves(game)
	if err != nil {
		return err
	}
	fmt.Println("Verified save backup:", backup)
	metadata, _ := json.MarshalIndent(struct {
		Version string `json:"version"`
		Backup  string `json:"backup"`
	}{overlayVersion(language), backup}, "", "  ")
	if err = os.WriteFile(filepath.Join(overlay, "manifest.json"), metadata, 0600); err != nil {
		return err
	}
	if err = os.WriteFile(filepath.Join(overlay, "ready"), []byte(overlayVersion(language)+"\n"), 0600); err != nil {
		return err
	}
	if err = os.Rename(overlay, active); err != nil {
		return err
	}
	progress("ready", 100)
	return nil
}
func run() int {
	game := flag.String("game", ".", "Installed game directory")
	mode := flag.String("mode", "probe", "probe, install, decline or localize")
	language := flag.String("language", "auto", "auto, fr or en")
	installer := flag.String("installer", "", "Optional local UAP installer")
	extracted := flag.String("extracted", "", "Extracted UAP directory for localization checks")
	output := flag.String("output", "", "Localization check output directory")
	flag.Parse()
	root, err := filepath.Abs(*game)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	c, err := loadCatalog()
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	if *language != "auto" && *language != "fr" && *language != "en" {
		fmt.Fprintln(os.Stderr, "unsupported language")
		return 1
	}
	if *mode == "probe" {
		if _, e := findCase(root, "Arcanum5.dat"); e == nil {
			return 0
		}
		if overlayReady(root, "fr") || overlayReady(root, "en") {
			return 0
		}
	}
	if *language == "auto" {
		edition, e := detectEdition(root, c)
		if e != nil {
			fmt.Println("UAP not offered:", e)
			if *mode == "probe" {
				return 0
			}
			return 1
		}
		*language = edition
	}
	work = filepath.Join(root, "modern-ui", "uap-"+*language)
	if *mode == "probe" {
		if b, e := os.ReadFile(filepath.Join(work, "choice")); e == nil && string(b) == "no\n" {
			return 0
		}
	}
	if err = os.MkdirAll(work, 0700); err != nil {
		fmt.Fprintln(os.Stderr, err)
		return 1
	}
	switch *mode {
	case "probe":
		edition, e := detectEdition(root, c)
		if e != nil || edition != *language {
			return 0
		}
		if edition == "en" {
			return 21
		}
		return 20
	case "decline":
		err = os.WriteFile(filepath.Join(work, "choice"), []byte("no\n"), 0600)
	case "install":
		exe, e := os.Executable()
		if e != nil {
			err = e
			break
		}
		seven := "7zz"
		if runtime.GOOS == "windows" {
			seven = "7z.exe"
		}
		err = prepare(root, *installer, filepath.Join(filepath.Dir(exe), seven), c, *language)
	case "localize":
		packed, _ := assets.ReadFile("data/plan.json.gz")
		r, e := gzip.NewReader(bytes.NewReader(packed))
		if e != nil {
			err = e
			break
		}
		raw, e := io.ReadAll(r)
		r.Close()
		if e != nil {
			err = e
			break
		}
		var file *os.File
		file, err = os.CreateTemp(work, "plan-")
		if err != nil {
			break
		}
		name := file.Name()
		defer os.Remove(name)
		_, err = file.Write(raw)
		file.Close()
		if err == nil {
			err = os.MkdirAll(*output, 0700)
		}
		if err == nil {
			err = buildFrench(root, *extracted, *output, name)
		}
	default:
		err = fmt.Errorf("unknown mode: %s", *mode)
	}
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		progress("failed", 0)
		return 1
	}
	return 0
}
func main() { os.Exit(run()) }
