package main

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"
	"strings"
	"time"
)

func (v view) busy() bool {
	return v.Phase == "install" || v.Phase == "uap-probe" || v.Phase == "uap-install"
}
func (v view) autoStart() bool { return v.Ready && !v.Fatal && v.Phase != "uap-failed" }
func (v view) playLabel(language string) string {
	if v.Offer != 0 {
		return localized(language, "Continuer sans installer", "Continue without installing")
	}
	if !v.Ready {
		return localized(language, "Jouer sans mise à jour", "Play without updating")
	}
	return localized(language, "Jouer", "Play")
}
func (v view) secondaryLabel(language string) string {
	if v.Offer != 0 {
		return localized(language, "Installer", "Install")
	}
	return localized(language, "Lancer le jeu automatiquement", "Launch the game automatically")
}
func (v view) heading(language string) string {
	if strings.HasPrefix(v.Phase, "uap-") {
		return "UAP 2.0.2 / Hotfixes 1.1.0"
	}
	s := localized(language, "NOTES DE VERSION", "RELEASE NOTES")
	if v.Tag != "" {
		s += " · " + v.Tag
	}
	return s
}
func (v view) notes(language string) string {
	if v.Offer != 0 {
		edition := localized(language, "anglais", "English")
		if v.GameLanguage == "fr" {
			edition = localized(language, "français", "French")
		}
		if v.Offer >= 22 {
			return fmt.Sprintf(localized(language,
				"Ajouter les UAP Hotfixes 1.1.0 en %s ?\r\n\r\nIls corrigent des dialogues, des objets et des zones. Votre installation UAP sera conservée.\r\n\r\nTéléchargement : moins de 1 Mo. Vos sauvegardes seront copiées et vérifiées avant activation. Les cartes HD devront être préparées à nouveau. Certaines corrections peuvent ne pas être rétroactives dans une partie existante.\r\n\r\nContinuer sans installer : mémoriser le refus et lancer le jeu sans ces correctifs.",
				"Add UAP Hotfixes 1.1.0 in %s?\r\n\r\nThey fix dialogues, items and areas. Your existing UAP installation will be preserved.\r\n\r\nDownload: less than 1 MB. Saved games will be backed up and verified before activation. HD maps will need to be prepared again. Some fixes may not apply retroactively to existing saves.\r\n\r\nContinue without installing: remember this choice and play without these hotfixes."), edition)
		}
		return fmt.Sprintf(localized(language,
			"Installer l'UAP 2.0.2 (Base + Race Mod) et les Hotfixes 1.1.0 en %s ?\r\n\r\nL'UAP modifie les quêtes, scripts, caractéristiques, objets et races. Une partie existante peut présenter des blocages ou des incompatibilités.\r\n\r\nVos sauvegardes seront copiées et vérifiées avant activation. Les archives d'origine seront conservées.\r\n\r\nTéléchargement : environ 485 Mo. Prévoyez 2 Go libres, plus la sauvegarde de vos parties. Les cartes HD devront être préparées à nouveau.\r\n\r\nContinuer sans installer : mémoriser le refus et lancer le jeu sans UAP.",
			"Install UAP 2.0.2 (Base + Race Mod) and Hotfixes 1.1.0 in %s?\r\n\r\nUAP changes quests, scripts, statistics, items and races. Existing campaigns may encounter blocked quests or incompatibilities.\r\n\r\nSaved games will be backed up and verified before activation. Original game archives are preserved.\r\n\r\nDownload: about 485 MB. Allow 2 GB of free space plus your save backup. HD maps will need to be prepared again.\r\n\r\nContinue without installing: remember this choice and play without UAP."), edition)
	}
	if v.Phase == "uap-failed" {
		return localized(language,
			"La préparation de l'UAP ou des Hotfixes n'a pas abouti. Aucun patch partiel n'est activé. Vous pouvez lancer le jeu avec les données déjà installées.\r\n\r\nConsultez ArcanumModern/launcher.log et le journal preparation.log dans modern-ui/uap-en, modern-ui/uap-fr ou modern-ui/uap-hotfix. Le contrôle initial est dans modern-ui/uap-probe.log.",
			"UAP or Hotfix preparation did not finish. No partial patch is activated. You can play using the data already installed.\r\n\r\nSee ArcanumModern/launcher.log and preparation.log under modern-ui/uap-en, modern-ui/uap-fr or modern-ui/uap-hotfix. The initial check is logged in modern-ui/uap-probe.log.")
	}
	if v.Notes == "" {
		return localized(language, "Les notes de la dernière release s’afficheront ici.", "The latest release notes will appear here.")
	}
	return v.Notes
}
func (a *application) respond(install bool) {
	a.mu.Lock()
	if a.state.Offer == 0 {
		a.mu.Unlock()
		return
	}
	a.state.Offer = 0
	a.state.Phase = "uap-install"
	a.mu.Unlock()
	a.answer <- install
}

// The helper remains the sole owner of edition detection, checksums, save
// backups and atomic overlay activation. UI language never selects game data.
func (a *application) prepareUAP(ctx context.Context) {
	if !preparationSupported() {
		return
	}
	name := "uap-helper"
	if runtime.GOOS == "windows" {
		name += ".exe"
	}
	exe := filepath.Join(a.root, "ArcanumModern", "uap", name)
	if !exists(exe) {
		a.record(fmt.Errorf("UAP helper missing: %s", exe))
		a.phase("uap-failed")
		return
	}
	previous := a.snapshot().Phase
	a.phase("uap-probe")
	code, err := a.uapHelper(ctx, exe, "probe", "auto", "")
	if ctx.Err() != nil {
		return
	}
	if err != nil {
		a.record(err)
		a.phase("uap-failed")
		return
	}
	if code == 0 {
		a.phase(previous)
		return
	}
	if code < 20 || code > 23 {
		a.record(fmt.Errorf("UAP probe exited with %d", code))
		a.phase("uap-failed")
		return
	}
	lang := "en"
	if code == 20 || code == 22 {
		lang = "fr"
	}
	a.set(func(v *view) { v.Phase = "uap-choice"; v.Offer = code; v.GameLanguage = lang; v.Percent = 0 })
	var install bool
	select {
	case install = <-a.answer:
	case <-ctx.Done():
		a.set(func(v *view) { v.Offer = 0 })
		return
	}
	if ctx.Err() != nil {
		return
	}
	mode := "decline"
	if install {
		mode = "install"
	}
	work := filepath.Join(a.root, "modern-ui", "uap-"+lang)
	if code >= 22 {
		mode += "-hotfix"
		work = filepath.Join(a.root, "modern-ui", "uap-hotfix", lang)
	}
	a.set(func(v *view) { v.Phase = "uap-install"; v.Stage = "check"; v.Percent = 0 })
	// Once accepted, let the helper finish its backup/activation transaction.
	code, err = a.uapHelper(context.Background(), exe, mode, lang, work)
	if err != nil || code != 0 {
		a.record(fmt.Errorf("UAP %s: exit %d, %v", mode, code, err))
		a.phase("uap-failed")
		return
	}
	a.phase(previous)
}
func (a *application) uapHelper(ctx context.Context, executable, mode, language, work string) (int, error) {
	logPath := filepath.Join(a.root, "modern-ui", "uap-probe.log")
	if work != "" {
		logPath = filepath.Join(work, "preparation.log")
	}
	if e := os.MkdirAll(filepath.Dir(logPath), 0700); e != nil {
		return -1, e
	}
	log, e := os.OpenFile(logPath, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
	if e != nil {
		return -1, e
	}
	defer log.Close()
	cmd := exec.CommandContext(ctx, executable, "--game", a.root, "--mode", mode, "--language", language)
	cmd.Dir = a.root
	cmd.Stdout = log
	cmd.Stderr = log
	quietHelper(cmd)
	if e = cmd.Start(); e != nil {
		return -1, e
	}
	done := make(chan error, 1)
	go func() { done <- cmd.Wait() }()
	tick := time.NewTicker(100 * time.Millisecond)
	defer tick.Stop()
	for {
		select {
		case e = <-done:
			if status, ok := e.(*exec.ExitError); ok {
				return status.ExitCode(), nil
			}
			return 0, e
		case <-tick.C:
			if work == "" {
				continue
			}
			data, err := os.ReadFile(filepath.Join(work, "progress"))
			fields := strings.Fields(string(data))
			if err == nil && len(fields) == 2 {
				p, err := strconv.Atoi(fields[1])
				if err != nil || p < 0 || p > 100 {
					continue
				}
				a.set(func(v *view) { v.Stage = fields[0]; v.Percent = p })
			}
		}
	}
}
