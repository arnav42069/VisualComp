# License Activation Flow - Fixed for Keygen "Must Have 1 Machine" Policy

## Problem
Keygen's license validation requires that each license key be associated with exactly one machine. The original flow was:
1. Validate key (Keygen rejects: "must have exactly 1 associated machine")
2. Try to register machine (never reached due to validation failure)

## Solution
Three-step activation flow implemented in `LicenseVerifier::verify()`:

### Step 1: Validate License Key
```
POST https://api.keygen.sh/v1/accounts/{accountId}/licenses/actions/validate-key
Body: { "meta": { "key": "..." } }
```
- Sends the license key to Keygen
- Extracts the license ID from response even if validation temporarily fails
- Continues if we got a license ID (don't abort on validation failure at this point)

### Step 2: Register Machine Under License
```
POST https://api.keygen.sh/v1/accounts/{accountId}/machines
Headers:
  Content-Type: application/vnd.api+json
  Accept: application/vnd.api+json
  Authorization: License {theUsersLicenseKey}
Body: {
  "data": {
    "type": "machines",
    "attributes": {
      "fingerprint": "...",
      "platform": "windows"
    },
    "relationships": {
      "license": {
        "data": { "type": "licenses", "id": "{licenseId}" }
      }
    }
  }
}
```
- Machines are created at the **top-level** `/machines` collection, with the owning
  license attached via a JSON:API relationship. The nested
  `/licenses/{licenseId}/machines` path is read-only and returns **404 Not Found**
  on POST — that was the original bug.
- Creating a machine requires authentication. We authenticate *as the license*
  using the end user's own key (`Authorization: License <key>`), so no admin token
  has to ship inside the binary.
- Generates unique hardware fingerprint (volume serial + CPU ID → MD5 hash)
- Registers this machine as the one authorized to use the license
- Handles 409 Conflict gracefully (machine already registered, idempotent)
- Recognizes "already exists" error messages as success

### Step 3: Retry Validation
```
POST https://api.keygen.sh/v1/accounts/{accountId}/licenses/actions/validate-key
Body: { "meta": { "key": "..." } }
```
- Retries the same validation endpoint
- Now succeeds because machine is registered under the license
- Returns both `licenseId` and `machineId` in result

## Code Changes

### LicenseVerifier::verify()
- NEW: Extracts license ID from Keygen response (even on temporary failures)
- NEW: Calls `registerMachine()` to associate fingerprint with license
- NEW: Retries validation after machine registration
- CHANGE: Returns result with both licenseId and machineId

### LicenseVerifier::registerMachine()
- Endpoint is the top-level `/v1/accounts/{accountId}/machines` collection.
  (An earlier attempt used the nested `/licenses/{licenseId}/machines` path,
  which 404s — Keygen does not accept POST there.)
- Request format is JSON:API: `data.type`, `data.attributes.fingerprint`,
  `data.attributes.platform`, and `data.relationships.license.data`
- Takes the license *key* as well as the license *ID*, so it can send
  `Authorization: License <key>` — Keygen rejects unauthenticated machine creation

### LicenseVerifier::parseKeygenResponse()
- CHANGE: Returns result even if validation fails (as long as license ID extracted)
- NEW: Caller can proceed to machine registration despite validation failure
- Comment: "we'll extract the license ID from the response anyway"

### LicenseVerifier::parseMachineRegistrationResponse()
- NEW: Handles HTTP 409 Conflict as success (machine already exists)
- NEW: Detects "already" and "machine" keywords in error messages
- NEW: Returns `isValid: true` for idempotent/duplicate registration
- CHANGE: `machineId` field set to "existing" on conflict (placeholder)

## Testing the Flow

1. **Launch the app**: `VisualComp 2.25.exe` (already running)
2. **Click demo watermark** to open license activation dialog
3. **Enter a valid Keygen license key** (from Keygen dashboard)
4. **Expected behavior**:
   - Machine fingerprint generated (visible in debug log)
   - Keygen API called 3 times:
     a. First validation (may fail with "machine" error)
     b. Machine registration (succeeds or returns 409)
     c. Retry validation (succeeds)
   - License ID and machine ID stored in `%APPDATA%\Azazel Audio\VisualComp 2\license.xml`
   - Dialog closes with "License activated" message
5. **Close and relaunch** the app
   - Should load license from disk
   - Should show "UNLOCKED" watermark instead of demo message

## Debugging

Enable JUCE debug logging to see API calls:
- `DBG("Validating license with Keygen...")` - first validation
- `DBG("Registering machine with Keygen")` - machine registration
- `DBG("Retrying license validation after machine registration")` - retry
- `DBG("License validation succeeded on retry")` - final success

Look for in the debug output:
```
Validating license with Keygen: VHM-...
API Endpoint: https://api.keygen.sh/v1/accounts/.../licenses/actions/validate-key
Extracted license ID: ...
Registering machine with Keygen
API Endpoint: https://api.keygen.sh/v1/accounts/.../machines
License ID: ...
Machine fingerprint: ...
Keygen machine registration response: {...}
Machine registered successfully. Machine ID: ...
Retrying license validation after machine registration
License validation succeeded on retry
```

## Related Files
- `src/LicenseVerifier.h` - VerificationResult struct (contains licenseId, machineId)
- `src/LicenseVerifier.cpp` - Implementation of 3-step flow
- `src/MachineFingerprint.h/.cpp` - Hardware fingerprint generation
- `src/LicenseManager.h/.cpp` - Persistence of license/machine IDs to XML
