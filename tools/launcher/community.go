package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"
	"unicode/utf8"
)

// Public API only. AWS credentials and mail destinations belong on the server.
// Set by the release build after the service and privacy notice are configured.
var communityURL = "https://3r6pol811l.execute-api.eu-west-3.amazonaws.com"

func communityRequest(route string, payload any) error {
	u, err := url.Parse(communityURL)
	if err != nil || u.Scheme != "https" || u.Host == "" || u.User != nil || u.RawQuery != "" || u.Fragment != "" {
		return fmt.Errorf("community service unavailable")
	}
	b, err := json.Marshal(payload)
	if err != nil {
		return err
	}
	client := &http.Client{Timeout: 5 * time.Second, CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse }}
	req, err := http.NewRequest("POST", strings.TrimRight(communityURL, "/")+route, bytes.NewReader(b))
	if err != nil {
		return err
	}
	req.Header.Set("Content-Type", "application/json")
	req.Header.Set("User-Agent", "ArcanumRenaissance")
	response, err := client.Do(req)
	if err != nil {
		return fmt.Errorf("community service unreachable")
	}
	defer response.Body.Close()
	_, _ = io.Copy(io.Discard, io.LimitReader(response.Body, 1024))
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return fmt.Errorf("community request rejected (%d)", response.StatusCode)
	}
	return nil
}

func sendFeedback(message string) error {
	message = strings.TrimSpace(message)
	if !utf8.ValidString(message) || utf8.RuneCountInString(message) < 10 || utf8.RuneCountInString(message) > 2000 {
		return fmt.Errorf("feedback must contain 10 to 2000 characters")
	}
	return communityRequest("/feedback", map[string]string{"message": message, "version": buildVersion})
}

func communityNotice(language string) string {
	return localized(language,
		"En cliquant sur Envoyer, vous transmettez votre message et la version du launcher au développeur d’Arcanum Renaissance par e-mail, via Amazon Web Services. Aucun journal, sauvegarde ou identifiant de votre ordinateur n’est joint. N’incluez pas de données personnelles dans votre message.\r\n\r\nL’adresse IP est nécessaire au transport réseau ; le service de feedback ne l’enregistre pas dans ses journaux applicatifs. Le texte est conservé uniquement dans la boîte mail du développeur pour traiter votre retour.",
		"Clicking Send emails your message and launcher version to the Arcanum Renaissance developer through Amazon Web Services. No logs, saves or computer identifiers are attached. Do not include personal information in your message.\r\n\r\nIP addresses are required for network transport; the feedback service does not record them in its application logs. The text is kept only in the developer’s mailbox to handle your feedback.")
}
