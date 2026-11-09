"""Feedback relay for the launcher. Do not log requests, tokens, message bodies or IPs."""
import base64
import json
import os
import re
import time
from datetime import datetime, timezone

import boto3
from botocore.exceptions import ClientError

ddb = boto3.client('dynamodb')
ses = boto3.client('sesv2')
TABLE = os.environ['TABLE_NAME']


def reply(status):
    return {'statusCode': status, 'headers': {'Cache-Control': 'no-store'}, 'body': ''}


def key(identity, day):
    return {'pk': {'S': identity}, 'sk': {'S': day}}


def budget(kind, maximum, now):
    day = datetime.fromtimestamp(now, timezone.utc).strftime('%Y-%m-%d')
    ddb.update_item(TableName=TABLE, Key=key('budget', kind + '#' + day),
                    UpdateExpression='SET expires = :ttl ADD submissions :one',
                    ConditionExpression='attribute_not_exists(submissions) OR submissions < :maximum',
                    ExpressionAttributeValues={':ttl': {'N': str(now + 172800)}, ':one': {'N': '1'}, ':maximum': {'N': str(maximum)}})


def record_error(code):
    """Bounded daily diagnostics; never store provider messages or request data."""
    if not isinstance(code, str) or not re.fullmatch(r'[A-Za-z0-9]{1,80}', code):
        code = 'UnknownError'
    now = int(time.time())
    day = datetime.fromtimestamp(now, timezone.utc).strftime('%Y-%m-%d')
    try:
        ddb.update_item(TableName=TABLE, Key=key('diagnostics', day + '#' + code),
                        UpdateExpression='SET expires = :ttl, last_seen = :now ADD failures :one',
                        ExpressionAttributeValues={':ttl': {'N': str(now + 172800)}, ':now': {'N': str(now)}, ':one': {'N': '1'}})
    except Exception:
        pass


def handler(event, context):
    try:
        if event.get('requestContext', {}).get('http', {}).get('method') != 'POST':
            return reply(405)
        body = event.get('body') or ''
        if len(body) > 16000:
            return reply(413)
        if event.get('isBase64Encoded'):
            body = base64.b64decode(body, validate=True).decode('utf-8')
        data = json.loads(body)
        if not isinstance(data, dict):
            return reply(400)
        route = event.get('rawPath', '')
        now = int(time.time())
        if route == '/feedback':
            if set(data) != {'message', 'version'}:
                return reply(400)
            message, version = data['message'], data['version']
            if not isinstance(message, str) or not 10 <= len(message.strip()) <= 2000:
                return reply(400)
            if not isinstance(version, str) or not re.fullmatch(r'[0-9A-Za-z.\-]{1,40}', version):
                return reply(400)
            budget('feedback', int(os.environ.get('DAILY_FEEDBACK_LIMIT', '50')), now)
            ses.send_email(FromEmailAddress=os.environ['FEEDBACK_FROM'],
                           Destination={'ToAddresses': [os.environ['FEEDBACK_TO']]},
                           Content={'Simple': {'Subject': {'Data': 'Arcanum Renaissance feedback', 'Charset': 'UTF-8'},
                                               'Body': {'Text': {'Data': 'Version: ' + version + '\n\n' + message.strip(), 'Charset': 'UTF-8'}}}})
            return reply(202)
        return reply(404)
    except (ValueError, TypeError, UnicodeError):
        return reply(400)
    except ClientError as error:
        code = error.response.get('Error', {}).get('Code', '')
        record_error(code)
        return reply(429 if code in ('ConditionalCheckFailedException', 'TransactionCanceledException', 'TooManyRequestsException') else 503)
    except Exception as error:
        # Never reflect provider diagnostics or destination addresses to clients.
        record_error(type(error).__name__)
        return reply(503)
