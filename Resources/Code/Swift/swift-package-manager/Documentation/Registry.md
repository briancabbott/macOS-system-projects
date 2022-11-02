# Swift Package Registry Service Specification

- [1. Notations](#1-notations)
- [2. Definitions](#2-definitions)
- [3. Conventions](#3-conventions)
  - [3.1. Application layer protocols](#31-application-layer-protocols)
  - [3.2. Authentication](#32-authentication)
  - [3.3. Error handling](#33-error-handling)
  - [3.4. Rate limiting](#34-rate-limiting)
  - [3.5. API versioning](#35-api-versioning)
  - [3.6. Package identification](#36-package-identification)
    - [3.6.1 Package scope](#361-package-scope)
    - [3.6.2. Package name](#362-package-name)
- [4. Endpoints](#4-endpoints)
  - [4.1. List package releases](#41-list-package-releases)
  - [4.2. Fetch information about a package release](#42-fetch-information-about-a-package-release)
    - [4.2.1. Package release resources](#421-package-release-resources)
    - [4.2.2. Package release metadata standards](#422-package-release-metadata-standards)
  - [4.3. Fetch manifest for a package release](#43-fetch-manifest-for-a-package-release)
    - [4.3.1. swift-version query parameter](#431-swift-version-query-parameter)
  - [4.4. Download source archive](#44-download-source-archive)
    - [4.4.1. Integrity verification](#441-integrity-verification)
    - [4.4.2. Download locations](#442-download-locations)
  - [4.5. Lookup package identifiers registered for a URL](#45-lookup-package-identifiers-registered-for-a-url)
  - [4.6. Create a package release](#46-create-a-package-release)
    - [4.6.1. Source archive](#461-source-archive)
    - [4.6.2. Package release metadata](#462-package-release-metadata)
      - [4.6.2.1. Reserved metadata keys](#4621-reserved-metadata-keys)
    - [4.6.3. Synchronous and asynchronous publication](#463-synchronous-and-asynchronous-publication)
      - [4.6.3.1. Synchronous publication](#4631-synchronous-publication)
      - [4.6.3.2. Asynchronous publication](#4632-asynchronous-publication)
- [5. Normative References](#5-normative-references)
- [6. Informative References](#6-informative-references)
- [Appendix A - OpenAPI Document](#appendix-a---openapi-document)

## 1. Notations

The following terminology and conventions are used in this document.

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT",
"SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL"
in this document are to be interpreted as described in [RFC 2119].

This specification uses the Augmented Backus-Naur Form (ABNF) notation
as described in [RFC 5234]
and Unicode regular expression syntax
as described in [Unicode Technical Standard #18][UAX18].

API endpoints that accept parameters in their path
are expressed by Uniform Resource Identifier (URI) templates,
as described in [RFC 6570].

## 2. Definitions

The following terms, as used in this document, have the meanings indicated.

- _Package_:
  A named collection of Swift source code
  that is organized into one or more modules
  according to a `Package.swift` manifest file.
- _Scope_:
  A logical grouping of related packages assigned by a package registry.
- _Release_:
  The state of a package after applying a particular set of changes
  that is uniquely identified by an assigned version number.
- _Version Number_:
  An identifier for a package release
  in accordance with the [Semantic Versioning Specification (SemVer)][SemVer].

## 3. Conventions

This document uses the following conventions
in its description of client-server interactions.

### 3.1. Application layer protocols

A client and server MUST communicate over a secured connection
using Transport Layer Security (TLS) with the `https` URI scheme.

The use of HTTP 1.1 in examples is non-normative.
A client and server MAY communicate according to this specification
using any version of the HTTP protocol.

### 3.2. Authentication

A server MAY require authentication
for client requests to access information about packages and package releases.

A server SHOULD respond with a status code of `401` (Unauthorized)
if a client sends a request to an endpoint that requires authentication
without providing credentials.
A server MAY respond with a status code of `404` (Not Found) or `403` (Forbidden)
when a client provides valid credentials
but isn't authorized to access the requested resource.

A server MAY use any authentication model of its choosing.
However, the use of a scoped, revocable authorization framework
like [OAuth 2.0][RFC 6749] is RECOMMENDED.

### 3.3. Error handling

A server MUST communicate any errors to the client
using "problem details" objects,
as described by [RFC 7807].
For example,
a client sends a request for a nonexistent release of a package
and receives the following response:

```http
HTTP/1.1 404
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "release not found"
}
```

### 3.4. Rate limiting

A server MAY limit the number of requests made by a client
by responding with a status code of `429` (Too Many Requests).

```http
HTTP/1.1 429
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en
Retry-After: 60

{
   "detail": "try again in 60 seconds"
}
```

A client SHOULD follow the guidance of any
`Retry-After` header values provided in responses
to prevent overwhelming a server with retry requests.
It is RECOMMENDED for clients to introduce random jitter in their retry logic
to avoid a [thundering herd effect].

### 3.5. API versioning

Package registry APIs are versioned.

API version numbers are designated by decimal integers.
The accepted version of this proposal constitutes the initial version, `1`.
Subsequent revisions SHOULD be numbered sequentially
(`2`, `3`, and so on).

API version numbers SHOULD follow
Semantic Versioning conventions for major releases.
Non-breaking changes, such as
adding new endpoints,
adding new optional parameters to existing endpoints,
or adding new information to existing endpoints in a backward-compatible way,
SHOULD NOT require a new version.
Breaking changes, such as
removing or changing an existing endpoint
in a backward-incompatible way,
MUST correspond to a new version.

A client SHOULD set the `Accept` header field
to specify the API version of a request.

```http
GET /mona/LinkedList/list HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1+json
```

Valid `Accept` header field values are described by the following rules:

```abnf
    version     = "1"       ; The API version
    mediatype   = "json" /  ; JSON (default media type)
                  "zip"  /  ; Zip archives, used for package releases
                  "swift"   ; Swift file, used for package manifest
    accept      = "application/vnd.swift.registry" [".v" version] ["+" mediatype]
```

A server MUST set the `Content-Type` and `Content-Version` header fields
with the corresponding content type and API version number of the response.

```http
HTTP/1.1 200 OK
Content-Type: application/json
Content-Version: 1
```

If a client sends a request without an `Accept` header,
a server MAY either respond with a status code of `400 Bad Request` or
process the request using an API version that it chooses,
making sure to set the `Content-Type` and `Content-Version` headers accordingly.

If a client sends a request with an `Accept` header
that specifies an unknown or invalid API version,
a server SHOULD respond with a status code of `400` (Bad Request).

```http
HTTP/1.1 400 Bad Request
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "invalid API version"
}
```

If a client sends a request with an `Accept` header
that specifies a valid but unsupported API version,
a server SHOULD respond with a status code of `415` (Unsupported Media Type).

```http
HTTP/1.1 415 Unsupported Media Type
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "unsupported API version"
}
```

### 3.6. Package identification

A package may declare external packages as dependencies in its manifest.
Each package dependency may specify a requirement
on which versions are allowed.

An external package dependency may itself have
one or more external package dependencies,
known as <dfn>transitive dependencies</dfn>.
When multiple packages have dependencies in common,
Swift Package Manager determines which version of that package should be used
(if any exist that satisfy all specified requirements)
in a process called <dfn>package resolution</dfn>.

Each external package is uniquely identified
by a scoped identifier in the form `scope.package-name`.

#### 3.6.1 Package scope

A *scope* provides a namespace for related packages within a package registry.
A package scope consists of alphanumeric characters and hyphens.
Hyphens may not occur at the beginning or end,
nor consecutively within a scope.
The maximum length of a package scope is 39 characters.
A valid package scope matches the following regular expression pattern:

```regexp
\A[a-zA-Z0-9](?:[a-zA-Z0-9]|-(?=[a-zA-Z0-9])){0,38}\z
```

Package scopes are case-insensitive
(for example, `mona` ≍ `MONA`).

#### 3.6.2. Package name

A package's *name* uniquely identifies a package in a scope.
A package name consists of alphanumeric characters, underscores, and hyphens.
Hyphens and underscores may not occur at the beginning or end,
nor consecutively within a name.
The maximum length of a package name is 100 characters.
A valid package name matches the following regular expression pattern:

```regexp
\A[a-zA-Z0-9](?:[a-zA-Z0-9]|[-_](?=[a-zA-Z0-9])){0,99}\z
```

Package names are case-insensitive
(for example, `LinkedList` ≍ `LINKEDLIST`).

## 4. Endpoints

A server MUST respond to the following endpoints:

| Link                 | Method | Path                                                      | Description                                       |
| -------------------- | ------ | --------------------------------------------------------- | ------------------------------------------------- |
| [\[1\]](#endpoint-1) | `GET`  | `/{scope}/{name}`                                         | List package releases                             |
| [\[2\]](#endpoint-2) | `GET`  | `/{scope}/{name}/{version}`                               | Fetch metadata for a package release              |
| [\[3\]](#endpoint-3) | `GET`  | `/{scope}/{name}/{version}/Package.swift{?swift-version}` | Fetch manifest for a package release              |
| [\[4\]](#endpoint-4) | `GET`  | `/{scope}/{name}/{version}.zip`                           | Download source archive for a package release     |
| [\[5\]](#endpoint-5) | `GET`  | `/identifiers{?url}`                                      | Lookup package identifiers registered for a URL   |
| [\[6\]](#endpoint-6) | `PUT`  | `/{scope}/{name}/{version}`                               | Create a package release                          |

A server SHOULD also respond to `HEAD` requests
for each of the specified endpoints.

A client MAY send an `OPTIONS` request with an asterisk (`*`)
to determine the permitted communication options for the server.
A server MAY respond with a `Link` header containing
an entry for the `service-doc` relation type
with a link to this document, and
an entry for the `service-desc` relation type
with a link to the OpenAPI specification.

* * *

<a name="endpoint-1"></a>

### 4.1. List package releases

A client MAY send a `GET` request
for a URI matching the expression `/{scope}/{name}`
to retrieve a list of the available releases for a particular package.
A client SHOULD set the `Accept` header with the value
`application/vnd.swift.registry.v1+json`
and MAY append the `.json` extension to the requested URI.

```http
GET /mona/LinkedList HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1+json
```

If a package is found at the requested location,
a server SHOULD respond with a status code of `200` (OK)
and the `Content-Type` header `application/json`.
Otherwise, a server SHOULD respond with a status code of `404` (Not Found).

A server SHOULD respond with a JSON document
containing the releases for the requested package.

```http
HTTP/1.1 200 OK
Content-Type: application/json
Content-Version: 1
Content-Length: 508
Link: <https://github.com/mona/LinkedList>; rel="canonical",
      <ssh://git@github.com:mona/LinkedList.git>; rel="alternate",
      <https://packages.example.com/mona/LinkedList/1.1.1>; rel="latest-version",
      <https://github.com/sponsors/mona>; rel="payment"

{
    "releases": {
        "1.1.1": {
            "url": "https://packages.example.com/mona/LinkedList/1.1.1"
        },
        "1.1.0": {
            "url": "https://packages.example.com/mona/LinkedList/1.1.0",
            "problem": {
                "status": 410,
                "title": "Gone",
                "detail": "this release was removed from the registry"
            }
        },
        "1.0.0": {
            "url": "https://packages.example.com/mona/LinkedList/1.0.0"
        }
    }
}
```

The response body MUST contain a JSON object
nested at a top-level `releases` key,
whose keys are version numbers for releases and
whose values are objects containing the following fields:

| Key       | Type   | Description                           | Requirement Level |
| --------- | ------ | ------------------------------------- | ----------------- |
| `url`     | String | The location of the release resource. | OPTIONAL          |
| `problem` | Object | A [problem details][RFC 7807] object. | OPTIONAL          |

A server MAY specify a URL for a release using the `url` key.
A client SHOULD locate a release using the value of the `url` key, if one is provided.
Otherwise, the client SHOULD locate a release
by expanding the URI Template `/{scope}/{name}/{version}` on the originating host.

A server SHOULD communicate the unavailability of a package release
using a ["problem details"][RFC 7807] object.
A client SHOULD consider any releases with an associated `problem`
to be unavailable for the purposes of package resolution.

A server SHOULD respond with
a link to the latest published release of the package if one exists,
using a `Link` header field with a `latest-version` relation.

A server SHOULD list releases in order of precedence,
starting with the latest version.
However, a client SHOULD NOT assume
any specific ordering of versions in a response.

A server MAY include a `Link` entry
with the `canonical` relation type
that locates the source repository of the package.

A server MAY include one or more `Link` entries
with the `alternate` relation type
for other source repository locations.

A server MAY paginate results by responding with
a `Link` header field that includes any of the following relations:

| Name    | Description                             |
| ------- | --------------------------------------- |
| `next`  | The immediate next page of results.     |
| `last`  | The last page of results.               |
| `first` | The first page of results.              |
| `prev`  | The immediate previous page of results. |

For example,
the `Link` header field in a response for the third page of paginated results:

```http
Link: <https://packages.example.com/mona/HashMap/5.0.3>; rel="latest-version",
      <https://packages.example.com/mona/HashMap?page=1>; rel="first",
      <https://packages.example.com/mona/HashMap?page=2>; rel="previous",
      <https://packages.example.com/mona/HashMap?page=4>; rel="next",
      <https://packages.example.com/mona/HashMap?page=10>; rel="last"
```

A server MAY respond with additional `Link` entries,
such as one with a `payment` relation for sponsoring a package maintainer.

<a name="endpoint-2"></a>

### 4.2. Fetch information about a package release

A client MAY send a `GET` request
for a URI matching the expression `/{scope}/{name}/{version}`
to retrieve information about a release.
A client SHOULD set the `Accept` header with the value
`application/vnd.swift.registry.v1+json`,
and MAY append the `.json` extension to the requested URI.

```http
GET /mona/LinkedList/1.1.1 HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1+json
```

If a release is found at the requested location,
a server SHOULD respond with a status code of `200` (OK)
and the `Content-Type` header `application/json`.
Otherwise, a server SHOULD respond with a status code of `404` (Not Found).

```http
HTTP/1.1 200 OK
Content-Version: 1
Content-Type: application/json
Content-Length: 720
Link: <https://packages.example.com/mona/LinkedList/1.1.1>; rel="latest-version",
      <https://packages.example.com/mona/LinkedList/1.0.0>; rel="predecessor-version"
{
  "id": "mona.LinkedList",
  "version": "1.1.1",
  "resources": [
    {
      "name": "source-archive",
      "type": "application/zip",
      "checksum": "a2ac54cf25fbc1ad0028f03f0aa4b96833b83bb05a14e510892bb27dea4dc812"
    }
  ],
  "metadata": { ... }
}
```

The response body MUST contain a JSON object containing the following fields:

| Key         | Type   | Description                               |
| ----------- | ------ | ----------------------------------------- |
| `id`        | String | The namespaced package identifier.        |
| `version`   | String | The package release version number.       |
| `resources` | Array  | The resources available for the release.  |
| `metadata`  | Object | Additional information about the release. |

A server SHOULD respond with a `Link` header containing the following entries:

| Relation              | Description                                                    |
| --------------------- | -------------------------------------------------------------- |
| `latest-version`      | The latest published release of the package                    |
| `successor-version`   | The next published release of the package, if one exists       |
| `predecessor-version` | The previously published release of the package, if one exists |

A link with the `latest-version` relation
MAY correspond to the requested release.

#### 4.2.1. Package release resources

Each element in the `resources` array is a JSON object with the following keys:

| Key        | Type    | Description                                                         |
| ---------- | ------- | ------------------------------------------------------------------- |
| `name`     | String  | The name of the resource.                                           |
| `type`     | String  | The content type of the resource.                                   |
| `checksum` | String  | A hexadecimal representation of the SHA256 digest for the resource. |

A resource object SHOULD have one of the following combinations of
`name` and `type` values:

| Name               | Content Type      | Description                        |
| ------------------ | ----------------- | ---------------------------------- |
| `source-archive`   | `application/zip` | An archive of package sources.     |

A release MUST NOT have more than a single resource object
with a given combination of `name` and `type` values.

#### 4.2.2. Package release metadata standards

A server MAY include metadata in its package release response.
It is RECOMMENDED that package metadata be represented in [JSON-LD]
according to a structured data standard.
For example,
this response using the [Schema.org] [SoftwareSourceCode] vocabulary:

```jsonc
{
  "id": "mona.LinkedList",
  "version": "1.1.1",
  "resources": [ ... ],
  "metadata": {
    "@context": ["http://schema.org/"],
    "@type": "SoftwareSourceCode",
    "name": "LinkedList",
    "description": "One thing links to another.",
    "keywords": ["data-structure", "collection"],
    "version": "1.1.1",
    "codeRepository": "https://github.com/mona/LinkedList",
    "license": "https://www.apache.org/licenses/LICENSE-2.0",
    "programmingLanguage": {
      "@type": "ComputerLanguage",
      "name": "Swift",
      "url": "https://swift.org"
    },
    "author": {
        "@type": "Person",
        "@id": "https://example.com/mona",
        "givenName": "Mona",
        "middleName": "Lisa",
        "familyName": "Octocat"
    }
  }
}
```

<a name="endpoint-3"></a>

### 4.3. Fetch manifest for a package release

A client MAY send a `GET` request for a URI matching the expression
`/{scope}/{name}/{version}/Package.swift`
to retrieve the package manifest for a release.
A client SHOULD set the `Accept` header with the value
`application/vnd.swift.registry.v1+swift`.

```http
GET /mona/LinkedList/1.1.1/Package.swift HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1+swift
```

If a release is found at the requested location,
a server SHOULD respond with a status code of `200` (OK)
and the `Content-Type` header `text/x-swift`.
Otherwise, a server SHOULD respond with a status code of `404` (Not Found).

```http
HTTP/1.1 200 OK
Cache-Control: public, immutable
Content-Type: text/x-swift
Content-Disposition: attachment; filename="Package.swift"
Content-Length: 361
Content-Version: 1
Link: <http://packages.example.com/mona/LinkedList/1.1.1/Package.swift?swift-version=4>; rel="alternate"; filename="Package@swift-4.swift"; swift-tools-version="4.0",
      <http://packages.example.com/mona/LinkedList/1.1.1/Package.swift?swift-version=4.2>; rel="alternate"; filename="Package@swift-4.2.swift"; swift-tools-version="4.0"

// swift-tools-version:5.0
import PackageDescription

let package = Package(
    name: "LinkedList",
    products: [
        .library(name: "LinkedList", targets: ["LinkedList"])
    ],
    targets: [
        .target(name: "LinkedList"),
        .testTarget(name: "LinkedListTests", dependencies: ["LinkedList"]),
    ],
    swiftLanguageVersions: [.v4, .v5]
)
```

A server SHOULD respond with a `Content-Length` header
set to the size of the manifest in bytes.

A server SHOULD respond with a `Content-Disposition` header
set to `attachment` with a `filename` parameter equal to
the name of the manifest file
(for example, "Package.swift").

It is RECOMMENDED for clients and servers to support
caching as described by [RFC 7234].

A server MUST include a `Link` header field
with a value for each version-specific package manifest file
in the release's source archive,
whose filename matches the following regular expression pattern:

```regexp
\APackage@swift-(\d+)(?:\.(\d+))?(?:\.(\d+))?.swift\z
```

Each link value SHOULD have the `alternate` relation type,
a `filename` attribute set to the version-specific package manifest filename
(for example, `Package@swift-4.swift`), and
a `swift-tools-version` attribute set to the [Swift tools version]
specified by the package manifest file
(for example, `4.0` for a manifest beginning with the comment
`// swift-tools-version:4.0`).

#### 4.3.1. swift-version query parameter

A client MAY specify a `swift-version` query parameter
to request a manifest for a particular version of Swift.

```http
GET /mona/LinkedList/1.1.1/Package.swift?swift-version=4.2 HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1+swift
```

If the package includes a file named
`Package@swift-{swift-version}.swift`,
the server SHOULD respond with a status code of `200` (OK)
and the content of that file in the response body.

```http
HTTP/1.1 200 OK
Cache-Control: public, immutable
Content-Type: text/x-swift
Content-Disposition: attachment; filename="Package@swift-4.2.swift"
Content-Length: 361
Content-Version: 1

// swift-tools-version:4.2
import PackageDescription

let package = Package(
    name: "LinkedList",
    products: [
        .library(name: "LinkedList", targets: ["LinkedList"])
    ],
    targets: [
        .target(name: "LinkedList"),
        .testTarget(name: "LinkedListTests", dependencies: ["LinkedList"]),
    ],
    swiftLanguageVersions: [.v3, .v4]
)
```

Otherwise,
the server SHOULD respond with a status code of `303` (See Other)
and redirect to the unqualified `Package.swift` resource.

```http
HTTP/1.1 303 See Other
Content-Version: 1
Location: https://packages.example.com/mona/LinkedList/1.1.1/Package.swift
```

<a name="endpoint-4"></a>

### 4.4. Download source archive

A client MAY send a `GET` request
for a URI matching the expression `/{scope}/{name}/{version}.zip`
to retrieve a release's source archive.
A client SHOULD set the `Accept` header with the value
`application/vnd.swift.registry.v1+zip`
and MUST append the `.zip` extension to the requested path.

```http
GET /mona/LinkedList/1.1.1.zip HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1+zip
```

If a release is found at the requested location,
a server SHOULD respond with a status code of `200` (OK)
and the `Content-Type` header `application/zip`.
Otherwise, a server SHOULD respond with a status code of `404` (Not Found).

```http
HTTP/1.1 200 OK
Accept-Ranges: bytes
Cache-Control: public, immutable
Content-Type: application/zip
Content-Disposition: attachment; filename="LinkedList-1.1.1.zip"
Content-Length: 2048
Content-Version: 1
Digest: sha-256=oqxUzyX7wa0AKPA/CqS5aDO4O7BaFOUQiSuyfepNyBI=
Link: <https://mirror-japanwest.example.com/mona-LinkedList-1.1.1.zip>; rel=duplicate; geo=jp; pri=10; type="application/zip"
```

A server MUST respond with a `Content-Length` header
set to the size of the archive in bytes.
A client SHOULD terminate any requests whose response exceeds
the expected content length.

A server MAY respond with a `Digest` header
containing a cryptographic digest of the source archive.

A server SHOULD respond with a `Content-Disposition` header
set to `attachment` with a `filename` parameter equal to the name of the package
followed by a hyphen (`-`), the version number, and file extension
(for example, "LinkedList-1.1.1.zip").

It is RECOMMENDED for clients and servers to support
range requests as described by [RFC 7233]
and caching as described by [RFC 7234].

#### 4.4.1. Integrity verification

A client MUST verify the integrity of a downloaded source archive using
the `checksum` value for the associated `source-archive` resource
in the response to `GET /{scope}/{name}/{version}`,
as described in [4.2.1](#421-package-release-resources).

A client SHOULD also verify the integrity using any values
provided in the `Digest` header of the source archive response
(for using the command
`shasum -b -a 256 LinkedList-1.1.1.zip | cut -f1 | xxd -r -p | base64`).

#### 4.4.2. Download locations

A server MAY specify mirrors or multiple download locations
using `Link` header fields
with a `duplicate` relation,
as described by [RFC 6249].
A client MAY use this information
to determine its preferred strategy for downloading.

A server MAY respond with a status code of `303` (See Other)
to redirect the client to download the source archive from another host.
The client MUST NOT follow redirects that downgrade to an insecure connection.
The client SHOULD limit the number of redirects to prevent a redirect loop.

For example,
a server redirects the client to download from
a content delivery network (CDN) using a signed URL:

```http
HTTP/1.1 303 See Other
Location: https://example.cdn.com/LinkedList-1.1.1.zip?key=XXXXXXXXXXXXXXXXX
```

```http
GET /LinkedList-1.1.1.zip?key=XXXXXXXXXXXXXXXXX HTTP/1.1
Host: example.cdn.com
Accept: application/vnd.swift.registry.v1+zip
```

```http
HTTP/1.1 200 OK
Accept-Ranges: bytes
Cache-Control: public, immutable
Content-Type: application/zip
Content-Disposition: attachment; filename="LinkedList-1.1.1.zip"
Content-Length: 2048
Content-Version: 1
Digest: sha-256=a2ac54cf25fbc1ad0028f03f0aa4b96833b83bb05a14e510892bb27dea4dc812
```

<a name="endpoint-5"></a>

### 4.5. Lookup package identifiers registered for a URL

A client MAY send a `GET` request
for a URI matching the expression `/identifiers{?url}`
to retrieve package identifiers associated with a particular URL.
A client SHOULD set the `Accept` header with the value
`application/vnd.swift.registry.v1+json`.

```http
GET /identifiers?url=https://github.com/mona/LinkedList HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1
```

If one or more package identifiers are associated with the specified URL,
a server SHOULD respond with a status code of `200` (OK)
and the `Content-Type` header `application/json`.
Otherwise, a server SHOULD respond with a status code of `404` (Not Found).

A server SHOULD respond with a JSON document
containing the package identifiers for the specified URL.

```http
HTTP/1.1 200 OK
Content-Type: application/json
Content-Version: 1

{
    "identifiers": [
      "mona.LinkedList"
    ]
}
```

The response body MUST contain an array of package identifier strings
nested at a top-level `identifiers` key.

It is RECOMMENDED for clients and servers to support
caching as described by [RFC 7234].

<a name="endpoint-6"></a>

### 4.6. Create a package release

A client MAY send a `PUT` request
for a URI matching the expression
`/{scope}/{name}/{version}`
to publish a release of a package.
A client MUST provide a body encoded as multipart form data
with the following sections:

| Key               | Content-Type       | Description                               | Requirement Level |
| ----------------- | ------------------ | ----------------------------------------- | ----------------- |
| `source-archive`  | `application/zip`  | The source archive of the package.        | REQUIRED          |
| `metadata`        | `application/json` | Additional information about the release. | OPTIONAL          |

A client MUST set a `Content-Type` header with the value `multipart/form-data`,
and a `Content-Length` header with the total size of the body in bytes.
A client SHOULD set the `Accept` header with the value
`application/vnd.swift.registry.v1+json`.

```http
PUT /mona/LinkedList?version=1.1.1 HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1+json
Content-Type: multipart/form-data;boundary="boundary"
Content-Length: 336
Expect: 100-continue

--boundary
Content-Disposition: form-data; name="source-archive"
Content-Type: application/zip
Content-Length: 32
Content-Transfer-Encoding: base64

gHUFBgAAAAAAAAAAAAAAAAAAAAAAAA==

--boundary
Content-Disposition: form-data; name="metadata"
Content-Type: application/json
Content-Transfer-Encoding: quoted-printable
Content-Length: 3

{ }

```

A server SHOULD require a client to perform authentication
for any requests to create a package release.
Use of multi-factor authentication is RECOMMENDED.

A client MAY publish releases in any order.
For example,
if a package has existing `1.0.0` and `2.0.0` releases,
a client MAY publish a new `1.0.1` or `1.1.0` release.

Once a release has been published,
any resources associated with that release,
including its source archive,
MUST NOT change.

If a release already exists for a package at the specified version,
the server SHOULD respond with a status code of `409` (Conflict).

```http
HTTP/1.1 409 Conflict
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "a release with version 1.0.0 already exists"
}
```

It is RECOMMENDED that a server institute policies
for publishing new releases of a package
after a scope is transferred to a new owner.
For example,
the next release of an existing package is published with a new major version,
or only after a period of 45 days after transfer.

If the client provides an `Expect` header,
a server SHOULD check that the request can succeed
before responding with a status code of `100 (Continue)`.
A server that doesn't support expectations
SHOULD respond with a status code of `417 (Expectation Failed)`.
In response,
a client MAY remove the `Expect` header and retry the request.

```http
HTTP/1.1 417 (Expectation Failed)
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "expectations aren't supported"
}
```

Support for this endpoint is OPTIONAL.
A server SHOULD indicate that publishing isn't supported
by responding with a status code of `405` (Method Not Allowed).

```http
HTTP/1.1 405 (Method Not Allowed)
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "publishing isn't supported"
}
```

A server MAY respond either synchronously or asynchronously.
For more information,
see [4.6.4](#464-synchronous-and-asynchronous-publication).

#### 4.6.1. Source archive

A client MUST include a multipart section named `source-archive`
containing the source archive for a release.
A client SHOULD set a `Content-Type` header with the value `application/zip`
and a `Content-Length` header with the size of the Zip archive in bytes.

```http
--boundary
Content-Disposition: form-data; name="source-archive"
Content-Type: application/zip
Content-Length: 32
Content-Transfer-Encoding: base64

gHUFBgAAAAAAAAAAAAAAAAAAAAAAAA==
```

A client SHOULD use the `swift package archive-source` tool
to create a source archive for the release.

A server MAY analyze a package to
assess its viability,
perform security testing,
or otherwise evaluate software quality.
A server MAY refuse to publish a package release for any reason
by responding with a status code of `422` (Unprocessable Entity).

```http
HTTP/1.1 422 Unprocessable Entity
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "package doesn't contain a valid manifest (Package.swift) file"
}
```

A server SHOULD use the `swift package compute-checksum` tool
to compute the checksum that's provided in response to
a client's subsequent request to [download the source archive](#endpoint-4)
for the release.

#### 4.6.2. Package release metadata

A client MAY include a multipart section named `metadata`
containing additional information about the release.
A client SHOULD set a `Content-Type` header with the value `application/json`
and a `Content-Length` header with the size of the JSON document in bytes.
It is RECOMMENDED that package release metadata be represented in [JSON-LD]
according to a structured data standard,
as discussed in [4.2.1](#421-package-release-metadata-data-standards).

```http
--boundary
Content-Disposition: form-data; name="metadata"
Content-Type: application/json
Content-Length: 620
Content-Transfer-Encoding: quoted-printable

{
  "@context": ["http://schema.org/"],
  "@type": "SoftwareSourceCode",
  "name": "LinkedList",
  "description": "One thing links to another.",
  "keywords": ["data-structure", "collection"],
  "version": "1.1.1",
  "codeRepository": "https://github.com/mona/LinkedList",
  "license": "https://www.apache.org/licenses/LICENSE-2.0",
  "programmingLanguage": {
    "@type": "ComputerLanguage",
    "name": "Swift",
    "url": "https://swift.org"
  },
  "author": {
      "@type": "Person",
      "@id": "https://github.com/mona",
      "givenName": "Mona",
      "middleName": "Lisa",
      "familyName": "Octocat"
  }
}

```

If a client doesn't provide a `metadata` section,
a server MAY populate the metadata for a release.
A client MAY request that a server not populate metadata automatically
by sending an empty JSON object (`{}`) as its request body.

If a client provides an invalid JSON document,
the server SHOULD respond with a status code of
`422` (Unprocessable Entity) or `413` (Payload Too Large)
and MAY communicate validation error details in the response body.

```http
HTTP/1.1 422 Unprocessable Entity
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en

{
   "detail": "invalid JSON provided for release metadata"
}
```

##### 4.6.2.1. Reserved metadata keys

These metadata keys have special meanings to the server and should be included if and only if their value satisifies their intended use:

| Key               | Description                                          | Server Use                             |
| ----------------- | ---------------------------------------------------- | -------------------------------------- |
| `repositoryURLs`  | An array of the package's source code repository URLs (e.g., `["https://github.com/mona/LinkedList", "ssh://git@github.com:mona/LinkedList.git"]`). | The mappings between package identifer and repository URL get recorded for the [lookup package identifiers by URL](#endpoint-5) endpoint. | 

#### 4.6.3. Synchronous and asynchronous publication

A server MAY respond to a request to publish a new package release
either synchronously or asynchronously.

A client MAY indicate their preference for asynchronous processing
with a `Prefer` header field containing the token `respond-async`
and an optional `wait` preference,
as described by [RFC 7240].

```http
PUT /mona/LinkedList/1.1.1 HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1
Prefer: respond-async, wait=300
```

##### 4.6.3.1. Synchronous publication

If processing is done synchronously,
the server MUST respond with a status code of `201` (Created)
to indicate that the package release was published.
This response SHOULD also contain
a `Location` header with a URL to the new release.

```http
HTTP/1.1 201 Created
Content-Version: 1
Location: https://packages.example.com/github.com/mona/LinkedList/1.1.1
```

A client MAY set a timeout to guarantee a timely response to each request.

##### 4.6.3.2. Asynchronous publication

If processing is done asynchronously,
the server MUST respond with a status code of `202` (Accepted)
to acknowledge that the request is being processed.
This response MUST contain a `Location` header
with a URL that the client can poll for progress updates
and SHOULD contain a `Retry-After` header
with an estimate of when processing is expected to finish.
A server MAY locate the status resource endpoint at a URI of its choosing.
However,
the use of a non-sequential, randomly-generated identifier is RECOMMENDED.

```http
HTTP/1.1 202 Accepted
Content-Version: 1
Location: https://packages.example.com/submissions/90D8CC77-A576-47AE-A531-D6402C4E33BC
Retry-After: 120
```

A client MAY send a `GET` request
to the location provided by the server in response to a publish request
to see the current status of that process.

```http
GET /submissions/90D8CC77-A576-47AE-A531-D6402C4E33BC HTTP/1.1
Host: packages.example.com
Accept: application/vnd.swift.registry.v1
```

If the asynchronous publish request is still processing,
the server SHOULD respond with a status code of `202` (Accepted) and
a `Retry-After` header with an estimate of when processing should finish.
A server MAY include additional details in the response body.

```http
HTTP/1.1 202 Accepted
Content-Version: 1
Content-Type: application/json
Retry-After: 120

{
  "status": "Processing (2/3 steps complete)",
  "steps": {
    {"name": "Validate metadata", "status": "complete"},
    {"name": "Verify package manifest", "status": "complete"},
    {"name": "Scan for vulnerabilities", "status": "pending"}
  }
}
```

If the asynchronous publish request is finished processing successfully,
the server SHOULD respond with a status code of `301` (Moved Permanently)
and a `Location` header with a URL to the package release.

```http
HTTP/1.1 301 Moved Permanently
Content-Version: 1
Location: https://packages.example.com/mona/LinkedList/1.1.1
```

If the asynchronous publish request failed,
the server SHOULD respond with an appropriate client error status code (`4xx`).

```http
HTTP/1.1 400 Bad Request
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en
Location: https://packages.example.com/submissions/90D8CC77-A576-47AE-A531-D6402C4E33BC

{
   "detail": "invalid package"
}
```

A client MAY send a `DELETE` request
to the location provided by the server in response to a publish request
to cancel that process.

If a request to publish a new package release were to fail,
a server MUST communicate that failure in the same way
if sending an immediate response
as it would if responding to a client polling for status.

If a client makes a request to publish a package release
to a server that is asynchronously processing a request to publish that release,
the server MUST respond with a status code of `409` (Conflict)

```http
HTTP/1.1 409 Conflict
Content-Version: 1
Content-Type: application/problem+json
Content-Language: en
Location: https://packages.example.com/submissions/90D8CC77-A576-47AE-A531-D6402C4E33BC

{
   "detail": "already processing a request to publish this package version"
}
```

If a client makes a request to publish a package release
to a server that finished processing a failed request to publish that release,
the server SHOULD try publishing that release again.
A server MAY refuse to fulfill a subsequent request to publish a package release
by responding with a status code of `409` (Conflict).

## 5. Normative References

* [RFC 2119]: Key words for use in RFCs to Indicate Requirement Levels
* [RFC 3230]: Instance Digests in HTTP
* [RFC 3986]: Uniform Resource Identifier (URI): Generic Syntax
* [RFC 3987]: Internationalized Resource Identifiers (IRIs)
* [RFC 5234]: Augmented BNF for Syntax Specifications: ABNF
* [RFC 5843]: Additional Hash Algorithms for HTTP Instance Digests
* [RFC 6249]: Metalink/HTTP: Mirrors and Hashes
* [RFC 6570]: URI Template
* [RFC 7159]: The JavaScript Object Notation (JSON) Data Interchange Format
* [RFC 7230]: Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing
* [RFC 7231]: Hypertext Transfer Protocol (HTTP/1.1): Semantics and Content
* [RFC 7233]: Hypertext Transfer Protocol (HTTP/1.1): Range Requests
* [RFC 7234]: Hypertext Transfer Protocol (HTTP/1.1): Caching
* [RFC 7240]: Prefer Header for HTTP
* [RFC 7578]: Returning Values from Forms: multipart/form-data
* [RFC 7807]: Problem Details for HTTP APIs
* [RFC 8288]: Web Linking
* [SemVer]: Semantic Versioning

## 6. Informative References

* [BCP 13] Media Type Specifications and Registration Procedures
* [RFC 6749]: The OAuth 2.0 Authorization Framework
* [RFC 8446]: The Transport Layer Security (TLS) Protocol Version 1.3
* [RFC 8631]: Link Relation Types for Web Services
* [JSON-LD]: A JSON-based Serialization for Linked Data
* [Schema.org]: A shared vocabulary for structured data.
* [OAS]: OpenAPI Specification

## Appendix A - OpenAPI Document

The following [OpenAPI (v3) specification][OAS] is non-normative,
and is provided for the convenience of
developers interested in building their own package registry.

```yaml
openapi: 3.0.0
info:
  title: Swift Package Registry
  version: "1"
externalDocs:
  description: Swift Evolution Proposal SE-0292
  url: https://github.com/apple/swift-evolution/blob/main/proposals/0292-package-registry-service.md
servers:
  - url: https://packages.swift.org
paths:
  "/{scope}/{name}":
    parameters:
      - $ref: "#/components/parameters/scope"
      - $ref: "#/components/parameters/name"
    get:
      tags:
        - Package
      summary: List package releases
      operationId: listPackageReleases
      parameters:
        - name: Content-Type
          in: header
          schema:
            type: string
            enum:
              - application/vnd.swift.registry.v1+json
      responses:
        "200":
          description: ""
          headers:
            Content-Version:
              $ref: "#/components/headers/contentVersion"
            Content-Length:
              schema:
                type: integer
          content:
            application/json:
              schema:
                $ref: "#/components/schemas/releases"
              examples:
                default:
                  $ref: "#/components/examples/releases"
        4XX:
          $ref: "#/components/responses/problemDetails"
  "/{scope}/{name}/{version}":
    parameters:
      - $ref: "#/components/parameters/scope"
      - $ref: "#/components/parameters/name"
      - $ref: "#/components/parameters/version"
    get:
      tags:
        - Release
      summary: Fetch release metadata
      operationId: fetchReleaseMetadata
      parameters:
        - name: Content-Type
          in: header
          schema:
            type: string
            enum:
              - application/vnd.swift.registry.v1+json
      responses:
        "200":
          description: ""
          headers:
            Content-Version:
              $ref: "#/components/headers/contentVersion"
            Content-Length:
              schema:
                type: integer
          content:
            application/json:
              schema:
                type: object
              examples:
                default:
                  $ref: "#/components/examples/metadata"
        4XX:
          $ref: "#/components/responses/problemDetails"
    put:
      tags:
        - Release
      summary: Publish package release
      operationId: publishPackageRelease
      parameters:
        - name: Content-Type
          in: header
          schema:
            type: string
            enum:
              - application/vnd.swift.registry.v1+json
      responses:
        "100":
          description: ""
        "201":
          description: ""
          headers:
            Content-Version:
              $ref: "#/components/headers/contentVersion"
            Content-Length:
              schema:
                type: integer
          content:
            application/json:
              schema:
                $ref: "#/components/schemas/releases"
              examples:
                default:
                  $ref: "#/components/examples/releases"
        "202":
          description: ""
          headers:
            Content-Version:
              $ref: "#/components/headers/contentVersion"
            Location:
              schema:
                type: string
            Retry-After:
              schema:
                type: integer
        4XX:
          $ref: "#/components/responses/problemDetails"
  "/{scope}/{name}/{version}/Package.swift":
    parameters:
      - $ref: "#/components/parameters/scope"
      - $ref: "#/components/parameters/name"
      - $ref: "#/components/parameters/version"
    get:
      tags:
        - Release
      summary: Fetch manifest for a package release
      operationId: fetchManifestForPackageRelease
      parameters:
        - name: Content-Type
          in: header
          schema:
            type: string
            enum:
              - application/vnd.swift.registry.v1+swift
        - $ref: "#/components/parameters/swift_version"
      responses:
        "200":
          description: ""
          headers:
            Cache-Control:
              schema:
                type: string
            Content-Disposition:
              schema:
                type: string
            Content-Length:
              schema:
                type: integer
            Content-Version:
              $ref: "#/components/headers/contentVersion"
            Link:
              schema:
                type: string
          content:
            text/x-swift:
              schema:
                type: string
              examples:
                default:
                  $ref: "#/components/examples/manifest"
        4XX:
          $ref: "#/components/responses/problemDetails"
  "/{scope}/{name}/{version}.zip":
    parameters:
      - $ref: "#/components/parameters/scope"
      - $ref: "#/components/parameters/name"
      - $ref: "#/components/parameters/version"
    get:
      tags:
        - Release
      summary: Download source archive
      operationId: downloadSourceArchive
      parameters:
        - name: Content-Type
          in: header
          schema:
            type: string
            enum:
              - application/vnd.swift.registry.v1+zip
      responses:
        "200":
          description: ""
          headers:
            Accept-Ranges:
              schema:
                type: string
            Cache-Control:
              schema:
                type: string
            Content-Disposition:
              schema:
                type: string
            Content-Length:
              schema:
                type: integer
            Content-Version:
              $ref: "#/components/headers/contentVersion"
            Digest:
              required: true
              schema:
                type: string
            Link:
              schema:
                type: string
          content:
            application/zip:
              schema:
                type: string
                format: binary
        3XX:
          $ref: "#/components/responses/redirect"
        4XX:
          $ref: "#/components/responses/problemDetails"
  /identifiers:
    parameters:
      - $ref: "#/components/parameters/url"
    get:
      tags:
        - Package
      summary: Lookup package identifiers registered for a URL
      operationId: lookupPackageIdentifiersByURL
      parameters:
        - name: Content-Type
          in: header
          schema:
            type: string
            enum:
              - application/vnd.swift.registry.v1+json
      responses:
        "200":
          description: ""
          headers:
            Content-Version:
              $ref: "#/components/headers/contentVersion"
          content:
            application/json:
              schema:
                $ref: "#/components/schemas/identifiers"
              examples:
                default:
                  $ref: "#/components/examples/identifiers"
        4XX:
          $ref: "#/components/responses/problemDetails"
components:
  schemas:
    releases:
      type: object
      example:
        releases:
          1.1.0: https://swift.pkg.github.com/mona/LinkedList/1.1.0
      properties:
        releases:
          type: object
      required:
        - releases
    identifiers:
      type: object
      example:
        identifiers:
          - "mona.LinkedList"
      properties:
        identifiers:
          type: array
      required:
        - identifiers
    problem:
      type: object
      externalDocs:
        url: https://tools.ietf.org/html/rfc7807
      example:
        instance: /account/12345/msgs/abc
        balance: 30
        type: https://example.com/probs/out-of-credit
        title: You do not have enough credit.
        accounts:
          - /account/12345
          - /account/67890
        detail: Your current balance is 30, but that costs 50.
      properties:
        type:
          type: string
          format: uriref
        title:
          type: string
        status:
          type: number
        instance:
          type: string
        detail:
          type: string
      required:
        - detail
        - instance
        - status
        - title
        - type
  responses:
    problemDetails:
      description: A client error.
      headers:
        Content-Version:
          $ref: "#/components/headers/contentVersion"
        Content-Language:
          schema:
            type: string
        Content-Length:
          schema:
            type: integer
      content:
        application/problem+json:
          schema:
            $ref: "#/components/schemas/problem"
    redirect:
      description: A server redirect.
      headers:
        Content-Version:
          $ref: "#/components/headers/contentVersion"
        Location:
          schema:
            type: string
        Digest:
          schema:
            type: string
        Content-Length:
          schema:
            type: integer
  parameters:
    scope:
      name: scope
      in: path
      required: true
      schema:
        type: string
        example: "mona"
        pattern: \A[a-zA-Z\d](?:[a-zA-Z\d]|-(?=[a-zA-Z\d])){0,38}\z
    name:
      name: name
      in: path
      required: true
      schema:
        type: string
        example: LinkedList
        pattern: \A[a-zA-Z0-9](?:[a-zA-Z0-9]|[-_](?=[a-zA-Z0-9])){0,99}\z
    version:
      name: version
      in: path
      required: true
      schema:
        type: string
        externalDocs:
          description: Semantic Version number
          url: https://semver.org
        example: 1.0.0-beta.1
        pattern: ^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$
    swift_version:
      name: swift-version
      in: query
      schema:
        type: string
        example: 1.2.3
        pattern: \d+(?:\.(\d+)){0,2}
    url:
      name: url
      in: query
      required: true
      schema:
        type: string
        format: url
        example: https://github.com/mona/LinkedList
  examples:
    releases:
      value:
        releases:
          1.1.1:
            url: https://swift.pkg.github.com/mona/LinkedList/1.1.1
          1.1.0:
            problem:
              status: 410
              title: Gone
              detail: this release was removed from the registry
            url: https://swift.pkg.github.com/mona/LinkedList/1.1.0
          1.0.0:
            url: https://swift.pkg.github.com/mona/LinkedList/1.0.0
    manifest:
      value: >-
        // swift-tools-version:5.0

        import PackageDescription


        let package = Package(
            name: "LinkedList",
            products: [
                .library(name: "LinkedList", targets: ["LinkedList"])
            ],
            targets: [
                .target(name: "LinkedList"),
                .testTarget(name: "LinkedListTests", dependencies: ["LinkedList"]),
            ],
            swiftLanguageVersions: [.v4, .v5]
        )
    metadata:
      value:
        keywords:
          - data-structure
          - collection
        version: 1.1.1
        "@type": SoftwareSourceCode
        author:
          "@type": Person
          "@id": https://github.com/mona
          middleName: Lisa
          givenName: Mona
          familyName: Octocat
        license: https://www.apache.org/licenses/LICENSE-2.0
        programmingLanguage:
          url: https://swift.org
          name: Swift
          "@type": ComputerLanguage
        codeRepository: https://github.com/mona/LinkedList
        "@context":
          - http://schema.org/
        description: One thing links to another.
        name: LinkedList
    identifiers:
      value:
        identifiers:
          - "mona.LinkedList"
  headers:
    contentVersion:
      required: true
      schema:
        type: string
        enum:
          - - "1"

```

[BCP 13]: https://tools.ietf.org/html/rfc6838 "Media Type Specifications and Registration Procedures"
[RFC 2119]: https://tools.ietf.org/html/rfc2119 "Key words for use in RFCs to Indicate Requirement Levels"
[RFC 3230]: https://tools.ietf.org/html/rfc5843 "Instance Digests in HTTP"
[RFC 3986]: https://tools.ietf.org/html/rfc3986 "Uniform Resource Identifier (URI): Generic Syntax"
[RFC 3987]: https://tools.ietf.org/html/rfc3987 "Internationalized Resource Identifiers (IRIs)"
[RFC 5234]: https://tools.ietf.org/html/rfc5234 "Augmented BNF for Syntax Specifications: ABNF"
[RFC 5843]: https://tools.ietf.org/html/rfc5843 "Additional Hash Algorithms for HTTP Instance Digests"
[RFC 6249]: https://tools.ietf.org/html/rfc6249 "Metalink/HTTP: Mirrors and Hashes"
[RFC 6570]: https://tools.ietf.org/html/rfc6570 "URI Template"
[RFC 6749]: https://tools.ietf.org/html/rfc6749 "The OAuth 2.0 Authorization Framework"
[RFC 7159]: https://tools.ietf.org/html/rfc7159 "The JavaScript Object Notation (JSON) Data Interchange Format"
[RFC 7230]: https://tools.ietf.org/html/rfc7230 "Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing"
[RFC 7231]: https://tools.ietf.org/html/rfc7231 "Hypertext Transfer Protocol (HTTP/1.1): Semantics and Content"
[RFC 7233]: https://tools.ietf.org/html/rfc7233 "Hypertext Transfer Protocol (HTTP/1.1): Range Requests"
[RFC 7234]: https://tools.ietf.org/html/rfc7234 "Hypertext Transfer Protocol (HTTP/1.1): Caching"
[RFC 7240]: https://tools.ietf.org/html/rfc7240 "Prefer Header for HTTP"
[RFC 7578]: https://tools.ietf.org/html/rfc7578 "Returning Values from Forms: multipart/form-data"
[RFC 7807]: https://tools.ietf.org/html/rfc7807 "Problem Details for HTTP APIs"
[RFC 8288]: https://tools.ietf.org/html/rfc8288 "Web Linking"
[RFC 8446]: https://tools.ietf.org/html/rfc8446 "The Transport Layer Security (TLS) Protocol Version 1.3"
[RFC 8631]: https://tools.ietf.org/html/rfc8631 "Link Relation Types for Web Services"
[IANA Link Relations]: https://www.iana.org/assignments/link-relations/link-relations.xhtml
[JSON-LD]: https://w3c.github.io/json-ld-syntax/ "JSON-LD 1.1: A JSON-based Serialization for Linked Data"
[SemVer]: https://semver.org/ "Semantic Versioning"
[Schema.org]: https://schema.org/
[SoftwareSourceCode]: https://schema.org/SoftwareSourceCode
[DUST]: https://doi.org/10.1145/1462148.1462151 "Bar-Yossef, Ziv, et al. Do Not Crawl in the DUST: Different URLs with Similar Text. Association for Computing Machinery, 17 Jan. 2009. January 2009"
[OAS]: https://swagger.io/specification/ "OpenAPI Specification"
[GitHub / Swift Package Management Service]: https://forums.swift.org/t/github-swift-package-management-service/30406
[RubyGems]: https://rubygems.org "RubyGems: The Ruby community’s gem hosting service"
[PyPI]: https://pypi.org "PyPI: The Python Package Index"
[npm]: https://www.npmjs.com "The npm Registry"
[crates.io]: https://crates.io "crates.io: The Rust community’s crate registry"
[CocoaPods]: https://cocoapods.org "A dependency manager for Swift and Objective-C Cocoa projects"
[thundering herd effect]: https://en.wikipedia.org/wiki/Thundering_herd_problem "Thundering herd problem"
[offline cache]: https://yarnpkg.com/features/offline-cache "Offline Cache | Yarn - Package Manager"
[XCFramework]: https://developer.apple.com/videos/play/wwdc2019/416/ "WWDC 2019 Session 416: Binary Frameworks in Swift"
[SE-0272]: https://github.com/apple/swift-evolution/blob/master/proposals/0272-swiftpm-binary-dependencies.md "Package Manager Binary Dependencies"
[Swift tools version]: https://github.com/apple/swift-package-manager/blob/9b9bed7eaf0f38eeccd0d8ca06ae08f6689d1c3f/Documentation/Usage.md#swift-tools-version-specification "Swift Tools Version Specification"
