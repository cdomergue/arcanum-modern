package main

import "runtime"

func targetName() string {
	switch runtime.GOOS {
	case "windows":
		return "windows-x86_64"
	case "darwin":
		return "macos-arm64"
	default:
		return "linux-x86_64"
	}
}
func archiveName() string {
	switch runtime.GOOS {
	case "windows":
		return "ArcanumModern-windows-x64.zip"
	case "darwin":
		return "ArcanumModern-macos-arm64.zip"
	default:
		return "ArcanumModern-linux-x64.zip"
	}
}
func gameRelative() string {
	switch runtime.GOOS {
	case "windows":
		return "arcanum-modern.exe"
	case "darwin":
		return "Arcanum Modern.app/Contents/MacOS/Arcanum Community Edition"
	default:
		return "arcanum-modern"
	}
}
func launcherRelative() string {
	if runtime.GOOS == "windows" {
		return "renaissance-launcher.exe"
	}
	return "renaissance-launcher"
}
func playRelative() string {
	switch runtime.GOOS {
	case "windows":
		return "Play Arcanum Renaissance.bat"
	case "darwin":
		return "Play Arcanum Renaissance.command"
	default:
		return "Play Arcanum Renaissance.sh"
	}
}
