# Swift Package Registry Compatibility Test Suite

Contains tools for building and testing Swift package registry server that implements
[SE-0292](https://github.com/apple/swift-evolution/blob/main/proposals/0292-package-registry-service.md) and [SE-0321](https://github.com/apple/swift-evolution/blob/main/proposals/0321-package-registry-publish.md).

### Compatibility Test Suite

The [`PackageRegistryCompatibilityTestSuite`](./Sources/PackageRegistryCompatibilityTestSuite) module provides the
`package-registry-compatibility` command-line tool for testing all APIs defined in the 
[service specification](https://github.com/apple/swift-package-manager/blob/main/Documentation/Registry.md).

### Registry Service Reference Implementation

The [`PackageRegistryExample`](./Sources/PackageRegistryExample) module is a demo server app that implements the
[package registry service specification](https://github.com/apple/swift-package-manager/blob/main/Documentation/Registry.md) and can be
deployed locally using Docker.

### Testing

Running the unit tests for the compatibility test suite requires a package registry server. The easiest way is to use docker:

```
docker-compose -f docker/docker-compose.yml -f docker/docker-compose.2004.55.yml run test
```
