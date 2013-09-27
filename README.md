novena-eeprom editor
====================

Novena boards contain a device-dependent descriptive EEPROM that defines
various parameters such as serial number, MAC address, and featureset.  This
program allows you to view and manipulate this EEPROM list.

Developers
----------

The structure of the EEPROM v1.0 is defined in novena_eeprom.h.  It is laid
out as a packed struct.
