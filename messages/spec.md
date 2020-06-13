Concord is a message driven system. We want a standard messaging format usable from multiple languages that also provides a canonical serialization form. The format must be simple, so that it can be reimplemented in multiple languages in a matter of hours or days, not weeks or months. The way to do this is to make the format as limited as possible. It is not nearly as expressible or full featured as other formats, and it isn't intended to be. It's the bare minimum and is only expected to be used to implement messages, not arbitrary data structures.

# Goals
 * Easy to understand binary format
 * Easy to implement in *any* language
 * Canonical serialization format
 * Ability to implement zero-copy serialization/deserialization if desired. This is implementation dependent.
 * Schema based definition with code generation for each implementation

# Primitive Data Types

* bool
* Unsigned Integers -  uint8, uint16, uint32, uint64
* Signed Integers - sint8, sint16, sint32, sint64
* string - UTF-8 encoded
* bytes -  an arbitrary byte buffer

# Compound Data Types

Compound data types may include primitive types and other compound types. We ensure canonical serialization by ordering

 * kvpair - Keys must be primitive types. Values can be any type
 * list - A homogeneous list of any type
 * map - A lexicographically sorted list of key-value pairs
 * oneof - A sum type (tagged union) containing exactly one of the given messages. oneof types cannot contain primitives or compount types, they can only refer to messages. This is useful for deserializing a set of related messages into a given wrapper type. oneofs cannot be recursive either, as the parsing must be capable of terminating.

# Comments

Comments must be on their own line and start with the `#` character. Leading whitespace is allowed.

 # Serialized Representations

 Each type has a specific one byte key in the serialized form.

* `bool` - `0x00` = False, `0x01` = True
* `uint8` - The value itself
* `uint16` - The value itself in little endian
* `uint32` - The value itself in little endian
* `uint64` - The value itself in little endian
* `sint8` - The value itself in little endian
* `sint16` - The value itself in little endian
* `sint32` - The value itself in little endian
* `sint64` - The value itself in little endian
* `string` - uint32 length followed by UTF-8 encoded data
* `bytes` - uint32 length followed by arbitrary bytes
* `kvpair` - primitive key followed by primitive or compound value
* `list` - uint32 length of list followed by N homogeneous primitive or compound elements
* `map` - serialized as a list of lexicographically sorted key-value pairs
* `oneof` - uint32 message id of the contained message followed by the message

# Schema Format

All messages start with the token `Msg`, followed by a space and the message name, followed by a
space the message id, followed by a space and opening brace, `{`. Each field is specified with
the type name, followed by a space, followed by the field name. After all field definitions, a
closing brace, `}` is added. All types must be *flat*. No nested messsage definitions are
allowed. For nesting, use an existing message name as the type or multiple compound types.

## Field type formats

### Primitive Types

* `bool <name>`
* `uint8 <name>`
* `uint16 <name>`
* `uint32 <name>`
* `uint64 <name>`
* `sint8 <name>`
* `sint16 <name>`
* `sint32 <name>`
* `sint64 <name>`
* `string <name>`

### Compound Types

* `kvpair <primitive_key_type> <val_type> <name>`

Keys of kvpairs must be primitive types. Values can be compound types. Therefore, it's permissible to have field definitions like the following:

```
kvpair uint64 list string user_tags
```

* `list <type> name`
Lists are homogeneous, but be made up of any type. Therefore, it's permissible to have field definitions like the following:

```
list kvpair int string users
```

* `map <primitive_key_type> <val_type> name`

Similar to kvpairs, and lists, map values may contain compound types. Therefore, it's permissible to have field definitions like the following.

```
map string map string uint64 employee_salaries_by_company
```

* `oneof { <message_name_1> <message_name_2> ... <message_name_N> }`

A oneof can only contain message names.

## Example

```
Msg NewViewElement 1 {
    uint16 replica_id
    bytes digest
}

Msg NewView 2 {
  uint16 view
  list NewViewElement element
}

Msg PrePrepare 3 {
    uint64 view
    uint64 sequence_number
    uint16_t flags
    bytes digest
    list bytes client_requests
}

Msg ConsensusMsg 4 {
    uint32 protocol_version
    bytes span
    oneof {
        NewView
        PrePrepare
    }
}

```


