# Authentication response protocol

This document describes the server-to-client authentication response that the native game client can receive. It does not enable authentication transport.

## Server-to-client format

Authentication responses are sent as a client message with this exact prefix and delimiter layout:

```text
APP|auth|<request>|<status>|<message>
```

The native client consumes `APP|...` control messages instead of displaying them in chat. For a valid auth event, it forwards only the non-credential event payload to the Java activity as:

```text
auth|<request>|<status>|<message>
```

Fields are bounded before the native-to-Java callback:

| Field | Limit | Meaning |
| --- | ---: | --- |
| `request` | 1–16 bytes | Correlates the response with a server-side request. |
| `status` | 1–16 bytes | Machine-readable result code. |
| `message` | 0–160 bytes | User-safe status text or an empty detail. |

The complete client-message payload is also limited by the existing client-message parser to 255 bytes. Delimiters are not escaped; therefore `request` and `status` must not contain `|`. The parser rejects malformed or over-limit auth events and consumes them without showing them in chat.

## Status codes

The current native client treats `status` as an opaque, bounded value. No authoritative list of server status codes exists in this repository. Servers should define and document their own values; clients should display the accompanying safe `message` without assuming an undocumented code. An implementation may use values such as `ok`, `error`, `invalid`, `exists`, or `rate_limited`, but those names are conventions, not a client-enforced contract.

## Credential transport is intentionally disabled

The login/register XML is currently a visual resource only. It is not inflated, attached to an activity, or reachable from the Java activity lifecycle. No client code submits username, email, password, or password confirmation through chat, `APP|auth|...`, JNI, Volley, HTTP, or any other path added by this feature. The server-to-client response bridge must remain limited to safe status events; credentials must never be placed in logs, chat messages, native payloads, or JNI arguments.

Layout path: `app/src/main/res/layout/tela_registro.xml`.

Current reachability: not reachable. No Java code calls `inflate`, `setContentView`, or `addContentView` for this layout.
