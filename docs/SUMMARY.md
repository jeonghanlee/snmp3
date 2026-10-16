# Summary

[Introduction](introduction.md)

# Concepts

- [Architecture](snmp-rewrite-architecture.md)
- [Address worker supervision](snmp-worker-supervision.md)
- [Record request behavior](snmp-record-behavior.md)
- [Native SNMP transport](snmp-native-transport.md)
- [Request performance](snmp-performance/evaluation.md)

# Procedures

- [Configure the module at startup](configure-procedure.md)
- [Start the IOC and check the workers](start-ioc-procedure.md)
- [Verify the worker transport](snmp-worker-verification.md)
- [Verify the native transport](snmp-native-verification.md)
- [Measure device request performance](snmp-performance/test-procedure.md)
- [Measure the module path against a fixture agent](snmp-performance/fixture-procedure.md)
- [Measure devices through the module database](snmp-performance/module-database-procedure.md)
- [Size a new device](snmp-performance/sizing-procedure.md)
- [Build the documentation book](book-build.md)

# Reference

- [Startup configuration](snmp-rewrite-config.md)
- [Module contract and record support](snmp-rewrite-contract.md)
- [Worker commands, limits and timers](snmp-worker-reference.md)
- [Native result outcomes and value types](snmp-native-reference.md)
- [Performance test reference](snmp-performance/test-reference.md)
- [Request performance results](snmp-performance/results.md)
- [Glossary](glossary.md)

# Appendix

- [Design decisions]()
  - [Base lifecycle ownership](decisions/snmp-base-lifecycle.md)
  - [Native transport ownership](decisions/snmp-native-transport.md)
  - [Startup configuration ownership](decisions/snmp-startup-configuration.md)
  - [Worker supervision](decisions/snmp-worker-supervision.md)
- [PowerNet-MIB 4.6.0 review](powernet-460-review/review.md)
