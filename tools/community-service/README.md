# Feedback relay

This service receives `POST /feedback` with two JSON strings, `message` and
`version`. It sends the message to one configured recipient using Amazon SES.
The client does not choose the recipient or sender. Messages contain 10–2000
characters; the request body is bounded. Responses contain no provider error
details, addresses or credentials.

## Deployment

Deploy `template.yaml` with CloudFormation in an EU AWS region. Package
`handler.py` at the root of a ZIP in a private S3 bucket in that region. Supply
`CodeBucket`, `CodeKey`, `FeedbackFrom` and `FeedbackTo` as stack parameters.
When sending from a verified domain, set `SenderIdentity` to that domain and
`FeedbackFrom` to the full sender address. The IAM policy restricts sending to
that address and the configured recipient, even when the domain is verified.
Keep parameter files outside version control. Both address parameters are
`NoEcho`; they remain visible to authorized AWS administrators in the Lambda
configuration and IAM policy.

Verify the sender identity with SES. In the SES sandbox, verify the recipient
as well. A relay with a single verified recipient can operate in the sandbox;
SES account quotas still apply. A dedicated verified domain is preferable for
mail delivery when one is available.

Set the public `communityURL` in `tools/launcher/community.go` to the stack's
`Endpoint` output. This URL is intentionally public. Never put an AWS key,
recipient address or shared secret in the launcher. Compile and validate each
platform using the normal launcher build process.

## Limits and privacy

The HTTP API has a two-request-per-second throttle with a burst of four.
The Lambda has two reserved concurrent executions. An atomic DynamoDB counter
limits accepted feedback attempts to 50 per UTC day, including attempts where
SES subsequently fails. The counter contains only a date, count and expiration;
it is not a player identifier or usage statistic. DynamoDB TTL removes expired
counters asynchronously after two days.
Service failures also increment a daily diagnostic counter with an error code
and last-occurrence time, expiring after two days. These records contain no
message text, email address, player identifier or provider error description.

There are no API access logs and the Lambda role has no CloudWatch logging
permissions. The handler does not print request contents or network addresses.
AWS necessarily handles network metadata to serve HTTPS requests. Messages are
not stored by the API or database; delivered messages remain in the developer's
mailbox. The operator must maintain an appropriate mailbox retention policy and
handle deletion requests. Do not attach game logs, saves or identifiers without
a separate explicit player action and updated notice.

This is a public feedback endpoint, not an authenticated support inbox. Anyone
can submit text or consume the daily allowance. The quota limits email volume,
but is not a hard AWS spending cap. Monitor SES delivery and AWS billing. The
Lambda role is restricted to its rate-limit table and the configured SES sender
and recipient. It has no access to game data or other AWS resources.

## Verification

Run `python3 tools/community-service/test_handler.py` for offline validation,
destination-injection rejection, daily-limit behavior and private error handling.
Run the Go launcher tests for message validation, HTTPS enforcement, payload
minimization and redirect rejection. Use one clearly labelled technical message
to verify a deployed relay; HTTP 202 means SES accepted it, not that it arrived
in the inbox. Check the inbox and spam folder separately.

The launcher does not collect player or game-launch statistics.
