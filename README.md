ebclient is a simple CLI program for accessing [EPWING](https://ja.wikipedia.org/wiki/EPWING) & [電子ブック](<https://ja.wikipedia.org/wiki/%E9%9B%BB%E5%AD%90%E3%83%96%E3%83%83%E3%82%AF_(%E8%A6%8F%E6%A0%BC)>) dictionaries.
It accepts queries from stdin and outputs results to stdout. It's used as the backend of
[EBWeb](https://github.com/sagan/EBWeb).

## Dependencies

- [libebu](http://green.ribbon.to/~ikazuhiro/dic/ebu.html). It's a fork of
  [libeb (EB ライブラリ)](https://www.mistys-internet.website/eb/) which adds UTF-8 support to the latter.
- [libmxml](https://github.com/michaelrsweet/mxml). Used for accessing xml format gaiji
  ([外字](https://ja.wikipedia.org/wiki/%E5%A4%96%E5%AD%97)) -
  unicode mapping files that some epwing dictioneries provide.
- libz (`apt-get install lib32z1-dev`)

## Build

Install build tools (`apt-get install build-essential libtool-bin`) and run `make` in src/ dir.
The dependencies must be compiled priorly and put to corresponding locations which are referenced in Makefile.

## Usage

`./ebclient <dicts_path>`

`<dicts_path>` is the dir where epwing dictionaries files are put at, e.g.:

```
dicts_path/
|---dict_A/
|------CATALOGS
|------subbook_1/
|------subbook_2/
|---dict_B/
|------CATALOGS
|------subbook_1/
|------subbook_2/
```

libeb project provides [appendix](https://www.mistys-internet.website/eb/appendix.html) (補助データ) files for
some known dictionaries. For epwing dictionary, put the appendix file in the "subbook" folder
(the dir where "honmon" file exists) and rename it to "furoku". For 電子ブック dictionary,
put it to the subbook folder (the dir where "start" file exists) and keep the original "appendix" name unchanged.

## Communication protocol

When started, ebclient output the flatten list of all subbooks of all dictionaries in dicts_path in json format (with a trailing `\n`), e.g.:

```
["広辞苑第六版","付属資料","ＮＨＫ　日本語発音アクセント辞典"]

```

Afterwards, it read queries from stdin line by line, and output results to stdout in the same order.

Basic (input) query format:

```
<subbook_index> <query_type> <keyword>
```

- `<subbook_index>` : the subbook index (0-based) in the flatten list to query
- `<query_type>`: 0: prefix match; 1: suffix match; 2: exact match

Basic (output) result format (json):

[heading1, text1, heading2, text2...]

There are other query formats, distinguished by the first char of query line. For example, query line starts with `d` read an audio (wav) content from dictionary. For more, read the codes.
