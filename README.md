# rime-dictionary-lookup-filter
A librime filter plugin that can lookup dictionaries and override current comments of candidates with comments provided by another dictionary

## installation

```bash
cd librime/plugins/
git clone https://github.com/TypeDuck-HK/rime-dictionary-lookup-filter.git dictionary_plugin
```

then compile librime

## usage

example: in jyutping.schema.yaml
```yaml
  filters:
    - simplifier@variants_hk
    - simplifier@trad_tw
    - simplifier
    - simplifier@emoji_suggestion
```
put `dictionary_lookup_filter` on below.

```yaml
  filters:
    - simplifier@variants_hk
    - simplifier@trad_tw
    - simplifier
    - simplifier@emoji_suggestion
    - dictionary_lookup_filter
```

*options for dictionary_lookup_filter*

```yaml
dictionary_lookup_filter:
    dictionary: anotherDict
```

`anotherDict.schema.yaml`

```yaml
# Rime schema
# encoding: utf-8

schema:
  schema_id: anotherDict
  name: anotherDict
  version: "0.1"
  author:
    - ZNL
  description: |
    a dictionary for compile a trie-dictioinary
switches:
  - name: ascii_mode
    reset: 0
    states: [ 中文, 英文 ]

engine:
  processors:
    - ascii_composer
  segmentors:
    - ascii_segmentor
  translators:
    - table_translator

translator:
  dictionary: anotherDict
```

`anotherDict.dict.yaml`

```yaml

---
name: anotherDict
version: "0.0"
sort: original
use_preset_vocabulary: false
...

(a place name) Hong Kong	香港|hoeng1gong2
```
