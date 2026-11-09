"""Offline contract tests: no AWS account, email or network required."""
import importlib.util
import json
import os
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import Mock

class ClientError(Exception):
    def __init__(self, code):
        self.response = {'Error': {'Code': code}}

sys.modules['boto3'] = types.SimpleNamespace(client=lambda _: Mock())
sys.modules['botocore'] = types.ModuleType('botocore')
sys.modules['botocore.exceptions'] = types.SimpleNamespace(ClientError=ClientError)
os.environ.update(TABLE_NAME='test', FEEDBACK_FROM='sender@example.invalid', FEEDBACK_TO='recipient@example.invalid')
spec = importlib.util.spec_from_file_location('service', Path(__file__).with_name('handler.py'))
service = importlib.util.module_from_spec(spec)
spec.loader.exec_module(service)

class FeedbackTest(unittest.TestCase):
    def setUp(self):
        service.ddb, service.ses = Mock(), Mock()

    def request(self, payload, route='/feedback'):
        return service.handler({'rawPath': route, 'requestContext': {'http': {'method': 'POST'}}, 'body': json.dumps(payload)}, None)

    def test_only_fixed_destination_and_explicit_text(self):
        self.assertEqual(self.request({'message': 'A useful feedback', 'version': '2.2.0'})['statusCode'], 202)
        call = service.ses.send_email.call_args.kwargs
        self.assertEqual(call['Destination'], {'ToAddresses': ['recipient@example.invalid']})
        self.assertNotIn('ReplyToAddresses', call)
        self.assertEqual(service.ddb.update_item.call_count, 1)

    def test_bad_messages_and_destination_injection(self):
        for message in ('', 'short', 'x' * 2001, 42):
            self.assertEqual(self.request({'message': message, 'version': '2.2.0'})['statusCode'], 400)
        self.assertEqual(self.request({'message': 'A useful feedback', 'version': '2.2.0', 'to': 'attacker@example.invalid'})['statusCode'], 400)
        service.ses.send_email.assert_not_called()

    def test_budget_failure_never_sends(self):
        service.ddb.update_item.side_effect = ClientError('ConditionalCheckFailedException')
        self.assertEqual(self.request({'message': 'A useful feedback', 'version': '2.2.0'})['statusCode'], 429)
        service.ses.send_email.assert_not_called()

    def test_service_failure_does_not_leak_diagnostics(self):
        service.ses.send_email.side_effect = RuntimeError('private recipient and credentials')
        result = self.request({'message': 'A useful feedback', 'version': '2.2.0'})
        self.assertEqual(result['statusCode'], 503)
        self.assertEqual(result['body'], '')
        diagnostic = service.ddb.update_item.call_args.kwargs
        self.assertEqual(diagnostic['Key']['pk']['S'], 'diagnostics')
        self.assertTrue(diagnostic['Key']['sk']['S'].endswith('#RuntimeError'))
        self.assertNotIn('private recipient', repr(diagnostic))

    def test_no_statistics_routes(self):
        for path in ('/launch', '/delete', '/anything'):
            self.assertEqual(self.request({'token': 'a' * 64}, path)['statusCode'], 404)
        service.ddb.update_item.assert_not_called()
        service.ses.send_email.assert_not_called()

    def test_large_and_malformed_requests(self):
        for body, expected in [('x' * 16001, 413), ('{', 400), ('[]', 400)]:
            result = service.handler({'requestContext': {'http': {'method': 'POST'}}, 'body': body}, None)
            self.assertEqual(result['statusCode'], expected)

if __name__ == '__main__':
    unittest.main()
