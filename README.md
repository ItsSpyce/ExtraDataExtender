# Extra Data Extender

An SKSE mod for adding extra data to references/items.

## Architecture

- Inventory items are assigned a unique ID that persists in the actual extra data
  - weapons get ExtraHealth
  - armors get ExtraEnchantment
- References are simply tied to a unique ID in the SKSE cosave
- All extra data is stored as a key-value pair of `EDEExtraData::GetType` and a serializable dictionary in LMDB
- When a save is loaded, all UID<->extra-data-dictionary will be stored in memory
- When a reference is loaded/initialized/actually gets work done on it, the extra data for that will be stored on a hot plate for being worked on

> Loading data

```mermaid
sequenceDiagram
  participant cosave as SKSE Cosave
  participant ede as ExtraDataExtender
  participant lmdb as LMDB
  participant refr as TESObjectREFR

  cosave->>ede: load all uniqueID-to-refID mappings and cosave GUID
  ede->>lmdb: get LMDB table matching cosave GUID
  lmdb->>ede: table ref, locked until next save
```

> Getting data

```mermaid
sequenceDiagram
  participant query as Query Reference Data
  participant ede as ExtraDataExtender
  participant hp as Hot Plate
  participant lmdb as LMDB

  query->>ede: fetch unique ID for refr if it exists
  ede->>hp: check hot-plate for unique ID entries
  hp->>ede: returns data if exists
  ede->>lmdb: double-back to LMDB for data
  lmdb->>ede: return all extra data records
  ede->>lmdb: if Actor, iterate over inventory items
  lmdb->>ede: returns item extra data, storing in hot-plate
  ede->>hp: insert into hot plate
  ede->>query: return data
```

> Unloading data

```mermaid
sequenceDiagram
  participant unload as Unload Reference Data
  participant ede as ExtraDataExtender
  participant hot as Hot Plate
  participant lmdb as LMDB

  unload->>ede: request refr unload
  ede->>hot: get all hot-plate data
  hot->>ede: return hot-plate data and store in cold-plate
  hot->>unload: success boolean
```

> Saving data

```mermaid
sequenceDiagram
  participant skse as SKSE Cosave
  participant ede as ExtraDataExtender
  participant hot as Hot Plate
  participant cld as Cold Plate
  participant lmdb as LMDB

  skse->>ede: request save
  ede->>hot: fetch hot-plate data
  ede->>cld: fetch cold-plate data
  ede->>lmdb: clone previous table with new GUID then INSERT/UPDATE all records
  ede->>cld: clear cold-plate
  ede->>skse: write to cosave buffer
```
