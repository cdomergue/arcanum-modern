package main

import "fmt"

func localized(language, fr, en string) string {
	if language == "fr" {
		return fr
	}
	return en
}

func statusText(v view, language string) string {
	switch v.Phase {
	case "check":
		return localized(language, "Recherche de mises à jour…", "Checking for updates…")
	case "download":
		return fmt.Sprintf(localized(language, "Téléchargement de %s · %d %%", "Downloading %s · %d%%"), v.Tag, v.Percent)
	case "verify":
		return localized(language, "Vérification des fichiers…", "Verifying downloaded files…")
	case "install":
		return localized(language, "Installation de la mise à jour…", "Installing the update…")
	case "current":
		return localized(language, "Votre jeu est à jour.", "Your game is up to date.")
	case "updated":
		return localized(language, "Mise à jour installée. Bon voyage !", "Update installed. Enjoy your journey!")
	case "skipped":
		return localized(language, "Prêt à jouer avec la version installée.", "Ready to play the installed version.")
	case "offline":
		return localized(language, "Vérification indisponible. Vous pouvez jouer.", "Update check unavailable. You can still play.")
	case "failed":
		return localized(language, "Mise à jour impossible. Version précédente conservée.", "Update unavailable. Your previous version is preserved.")
	case "running":
		return localized(language, "Fermez le jeu avant de le mettre à jour.", "Close the game before updating.")
	case "recovery":
		return localized(language, "Restauration incomplète. Consultez launcher.log.", "Recovery incomplete. Please check launcher.log.")
	}
	return ""
}
