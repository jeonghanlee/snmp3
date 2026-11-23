# Native result outcomes and value types

Every admitted native operation produces one terminal completion with one outcome. This page lists the outcomes, the value types, and the representation limits that the native transport applies.

## Scope

This page covers the result outcomes, the checks on responses, the value type contract, and the object identifier (OID) limits of the native transport.

**Out of scope:** the ownership and lifetime rules (see [Native SNMP transport](snmp-native-transport.md)); the steps that run the verification (see [Verify the native transport](snmp-native-verification.md)).

Abbreviations on this page: SNMP is the Simple Network Management Protocol.

## Outcomes of a native result

| Outcome | Meaning |
| --- | --- |
| Complete | Accepted response with owned positional values |
| Timeout | Native request exhausted its actual deadline and retries, and no unparsable reply was counted |
| OpenFailure | Local or native open failed; no application request admitted |
| SendFailure | Native send failed; the caller-owned PDU is released |
| SecurityFailure | Native security-error callback or real key-setup failure |
| ProtocolFailure | Response carries an SNMP error status and index |
| SecurityConflict | Process-pinned user-based security model (USM) material disagrees |
| CapabilityMismatch | Immutable catalogue disagrees with the actual native APIs |
| ResponseRejected | Response identity, position, tag, or storage is invalid |
| Cancelled | Session or owner closed with the request pending |
| InvalidRequest | Batch, binding, payload, or native representation is invalid |
| Unreadable | Native request timed out while the library counted a reply that it could not parse |

Open and admission errors use `NativeError`. An admitted operation produces one terminal completion. Wrong credentials can produce a native timeout rather than SecurityFailure. A particular key-derivation failure is qualified only when an actual native call fails, because an open failure does not prove it.

## The message size of a session

After a session opens, the library reads `snmpEngineMaxMessageSize.0` of the unit (SNMP-FRAMEWORK-MIB) with one synchronous GET, with the timeout and the retries of the profile, for every version. `NativeIdentity.unitMessageSize` holds the value, 0 when it was not read: no reply, an error status, an exception, or a value that is not a positive integer. The read never fails the open. `NativeIdentity.messageSize` holds the message size of the session, which an SNMPv3 request carries as `msgMaxSize`: the lower of the value read and the configured `maxMessageSize`, never below 484, and the configured size when the value was not read. The worker returns `unitMessageSize` with every completed result of the session, and the IOC process shows it per endpoint.

## Checks on a native response

- Error status and index stay separate from values.
- Response count, positional OID, and storage and type checks precede copying; a GETBULK reply is checked as the bullet on GETBULK below says.
- Repeated requested OIDs remain distinct positions.
- The transport rejects missing, extra, reordered, or inconsistent duplicate variable bindings (varbinds) in the reply to a GET or a SET.
- A GET batch of consecutive instances of one column of an SNMPv2c or SNMPv3 endpoint is sent as one GETBULK that names the instance before the first one and asks for one row per binding. Its reply is not checked by position: each row reaches the binding that has its OID, a row the unit left out is NoSuchInstance for its binding, a row that is not of the column or the end of the MIB view ends the rows and the bindings left are NoSuchInstance, a reply that stops short of the rows asked for with neither of these ends as ProtocolFailure with the error status 1 (tooBig) and the error index 0, and rows that repeat or whose last arc does not rise end as ResponseRejected. An error index is checked against the one variable of the request.
- Exceptions keep the NoSuchObject, NoSuchInstance, and EndOfMibView tags.
- Intermediate REPORT and RESEND observations are not successful data.

## Supported value types

| Value | Native transport contract |
| --- | --- |
| Integer | Integer32 range, even though the owned Value can store signed 64-bit |
| Unsigned32, Gauge32 | Same ASN wire tag; the expected binding supplies the owned tag |
| Counter32, TimeTicks | Distinct native tags, unsigned 32-bit range |
| Counter64 | Exact high and low 32-bit native fields, owned unsigned 64-bit value |
| Octets | Owned bytes including NUL, bounded by binding capacity |
| OID | 2 to 128 arcs, 32-bit native subidentifiers, and binding capacity |
| IPv4 | Exactly four native bytes |
| Opaque Float32, Float64 | Exact native storage width and retained floating bits |

## Limits of the representation

On the verified Linux x86_64 native selection, `MAX_SUBID` is 4294967295. The native OID parser also limits `firstArc * 40 + secondArc` to that value. The transport rejects unsupported SET and binding OIDs before dispatch. The broader owned arc representation of Config does not bypass this native restriction.

The setting `checkRanges=false` cannot bypass wire or storage safety. The library reports native truncation as observed output and never reconstructs lost integer bits. EPICS device support applies additional [record conversion and storage rules](snmp-rewrite-contract.md#conversion-and-storage-rules).
