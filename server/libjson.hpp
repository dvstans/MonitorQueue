#ifndef LIBJSON_HPP
#define LIBJSON_HPP

#include <stdexcept>
#include <vector>
#include <map>
#include <string>
#include <stdint.h>

// This is the external entry point to the floating point conversion function
int fpconv_dtoa(double fp, char dest[24]);

namespace libjson {
    namespace detail {
        const char * STR_ERR_INVALID_CHAR           = "Invalid character";
        const char * STR_ERR_UNTERMINATED_ARRAY     = "Unterminated array";
        const char * STR_ERR_UNTERMINATED_OBJECT    = "Unterminated object";
        const char * STR_ERR_UNTERMINATED_VALUE     = "Unterminated value";
        const char * STR_ERR_INVALID_VALUE          = "Invalid value";
        const char * STR_ERR_INVALID_KEY            = "Invalid key string";
        const char * STR_ERR_INVALID_UNICODE        = "Invalid unicode escape sequence";
    }

    class Value;

    /**
    * @class ParseError
    * @brief Parse exception class to capture parse offset of error
    */
    class ParseError : public std::exception {
    public:
        ParseError( const char* a_msg, const char * a_pos ) : m_msg( a_msg ), m_pos( (size_t)a_pos ) {}

        const char * what() const noexcept {
            if ( !m_buf.size() ) {
                m_buf = std::string( m_msg ) + " at position " + std::to_string( m_pos );
            }
            return m_buf.c_str();
        }

        size_t getPos() {
            return m_pos;
        }

    private:
        void setOffset( const char * a_offset ) {
            m_pos -= (size_t)a_offset;
        }

        const char *        m_msg;
        size_t              m_pos;
        mutable std::string m_buf;

        friend class Value;
    };

    class Value {
    public:
        typedef std::map<std::string, Value>::iterator ObjectIter;
        typedef std::map<std::string, Value>::const_iterator ObjectConstIter;
        typedef std::vector<Value> Array;
        typedef std::string String;
        typedef std::vector<Value>::iterator ArrayIter;
        typedef std::vector<Value>::const_iterator ArrayConstIter;

        enum ValueType : uint8_t {
            VT_NULL = 0,
            VT_OBJECT,
            VT_ARRAY,
            VT_STRING,
            VT_NUMBER,
            VT_BOOL
        };

        /**
        * @class Object
        * @brief Provides a wrapper around underlying map to provide helper methods
        */
        class Object {
        public:
            Object() {
                m_iter = m_map.end();
            }

            ~Object() {}

            inline size_t size() {
                return m_map.size();
            }

            inline void clear() {
                m_map.clear();
                m_iter = m_map.end();
            }

            // The following methods look-up a value from key and attempt to return a specific type

            Value & getValue( const std::string& a_key ) {
                ObjectIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                return (Value &) iter->second;
            }

            const Value & getValue( const std::string& a_key ) const {
                ObjectConstIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                return iter->second;
            }

            Object & getObject( const std::string& a_key ) {
                ObjectIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_OBJECT ) {
                    return (Object &)*iter->second.m_value.o;
                }

                throw iter->second.typeConversionError( a_key, "object" );
            }

            const Object& getObject( const std::string& a_key ) const {
                ObjectConstIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_OBJECT )
                    return (const Object&)*iter->second.m_value.o;

                throw iter->second.typeConversionError( a_key, "object" );
            }

            Array& getArray( const std::string& a_key ) {
                ObjectIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_ARRAY ) {
                    return *iter->second.m_value.a;
                }

                throw iter->second.typeConversionError( a_key, "array" );
            }

            const Array& getArray( const std::string& a_key ) const {
                ObjectConstIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_ARRAY ) {
                    return *iter->second.m_value.a;
                }

                throw iter->second.typeConversionError( a_key, "array" );
            }


            bool getBool( const std::string& a_key ) const {
                ObjectConstIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_BOOL ) {
                    return iter->second.m_value.b;
                } else if ( iter->second.m_type == VT_NUMBER ) {
                    return (bool)iter->second.m_value.n;
                }

                throw iter->second.typeConversionError( a_key, "bool" );
            }

            double getNumber( const std::string& a_key ) const {
                ObjectConstIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_NUMBER ) {
                    return iter->second.m_value.n;
                } else if ( iter->second.m_type == VT_BOOL ) {
                    return iter->second.m_value.b ? 1 : 0;
                }

                throw iter->second.typeConversionError( a_key, "number" );
            }

            const std::string& getString( const std::string& a_key ) const {
                ObjectConstIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_STRING ) {
                    return *iter->second.m_value.s;
                }

                throw iter->second.typeConversionError( a_key, "string" );
            }

            std::string& getString( const std::string& a_key ) {
                ObjectIter iter = m_map.find( a_key );

                if ( iter == m_map.end() ) {
                    throw std::runtime_error( std::string( "Key not found: " ) + a_key );
                }

                if ( iter->second.m_type == VT_STRING ) {
                    return *iter->second.m_value.s;
                }

                throw iter->second.typeConversionError( a_key, "string" );
            }

            // Checks if key is present, sets internal iterator to entry

            inline bool has( const std::string& a_key ) const {
                return (m_iter = m_map.find( a_key )) != m_map.end();
            }

            // The following methods can be called after has() (sets internal iterator to entry)

            Value & value() {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                return (Value &) m_iter->second;
            }

            const Value & value() const {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                return m_iter->second;
            }

            ValueType type() const {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                return m_iter->second.m_type;
            }

            std::string & asString() {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                if ( m_iter->second.m_type == VT_STRING ) {
                    return *m_iter->second.m_value.s;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "string" );
            }

            const std::string& asString() const {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                if ( m_iter->second.m_type == VT_STRING ) {
                    return *m_iter->second.m_value.s;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "string" );
            }

            double asNumber() const {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "No key set" );
                }

                if ( m_iter->second.m_type == VT_NUMBER ) {
                    return m_iter->second.m_value.n;
                } else if ( m_iter->second.m_type == VT_BOOL ) {
                    return m_iter->second.m_value.b ? 1 : 0;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "number" );
            }

            bool asBool() const {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                if ( m_iter->second.m_type == VT_BOOL ) {
                    return m_iter->second.m_value.b;
                } else if ( m_iter->second.m_type == VT_NUMBER ) {
                    return (bool)m_iter->second.m_value.n;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "boolean" );
            }

            Object& asObject() {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                if ( m_iter->second.m_type == VT_OBJECT ) {
                    return *m_iter->second.m_value.o;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "object" );
            }

            const Object& asObject() const {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                if ( m_iter->second.m_type == VT_OBJECT ) {
                    return *m_iter->second.m_value.o;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "object" );
            }

            Array& asArray() {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                if ( m_iter->second.m_type == VT_ARRAY ) {
                    return *m_iter->second.m_value.a;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "array" );
            }

            const Array& asArray() const {
                if ( m_iter == m_map.end() ) {
                    throw std::runtime_error( "Key not set" );
                }

                if ( m_iter->second.m_type == VT_ARRAY ) {
                    return *m_iter->second.m_value.a;
                }

                throw m_iter->second.typeConversionError( m_iter->first, "array" );
            }

            // The following methods provide a lower-level map-like interface

            inline ObjectIter find( const std::string& a_key ) {
                return m_map.find( a_key );
            }

            inline ObjectConstIter find( const std::string& a_key ) const {
                return m_map.find( a_key );
            }

            inline ObjectIter begin() {
                return m_map.begin();
            }

            inline ObjectConstIter begin() const {
                return m_map.begin();
            }

            inline ObjectIter end() {
                return m_map.end();
            }

            inline ObjectConstIter end() const {
                return m_map.end();
            }

            Value& operator[]( const std::string& a_key ) {
                return m_map[a_key];
            }

            Value& at( const std::string& a_key ) {
                ObjectIter iter = m_map.find( a_key );

                if ( iter != m_map.end()) {
                    return iter->second;
                }

                throw std::runtime_error( std::string( "Key " ) + a_key + " not present" );
            }

            const Value& at( const std::string& a_key ) const {
                ObjectConstIter iter = m_map.find( a_key );

                if ( iter != m_map.end() ) {
                    return iter->second;
                }

                throw std::runtime_error( std::string( "Key " ) + a_key + " not present" );
            }

            void erase( const std::string& a_key ) {
                m_map.erase( a_key );
            }

        private:
            std::map<std::string, Value>    m_map;      ///< Map containing object key-value pairs
            mutable ObjectConstIter         m_iter;     ///< Internal iterator for has() / asType() methods
        };


        Value() : m_type( VT_NULL ), m_value( { 0 } ) {}

        explicit Value( bool a_value ) : m_type( VT_BOOL ) {
            m_value.b = a_value;
        }

        explicit Value( double a_value ) : m_type( VT_NUMBER ) {
            m_value.n = a_value;
        }

        explicit Value( int a_value ) : m_type( VT_NUMBER ) {
            m_value.n = a_value;
        }

        explicit Value( const std::string& a_value ) : m_type( VT_STRING ) {
            m_value.s = new String( a_value );
        }

        explicit Value( const char* a_value ) : m_type( VT_STRING ) {
            m_value.s = new String( a_value );
        }

        /// No copy - would require expensive deep-copy for object and array
        Value( const Value& a_source ) = delete;

        // Move ctor
        Value( Value&& a_source ) : m_type( a_source.m_type ), m_value( a_source.m_value ) {
            a_source.m_type = VT_NULL;
            a_source.m_value.o = 0;
        }

        explicit Value( ValueType a_type ) : m_type( a_type ) {
            if ( m_type == VT_OBJECT ) {
                m_value.o = new Object();
            } else if ( m_type == VT_ARRAY ) {
                m_value.a = new Array();
            } else if ( m_type == VT_STRING ) {
                m_value.s = new String();
            } else {
                m_value.o = 0;
            }
        }

        ~Value() {
            if ( m_type == VT_STRING ) {
                delete m_value.s;
            } else if ( m_type == VT_OBJECT ) {
                delete m_value.o;
            } else if ( m_type == VT_ARRAY ) {
                delete m_value.a;
            }
        }

        // Move assignment
        Value& operator=( Value&& a_source ) {
            if ( this != &a_source ) {
                ValueType   type = a_source.m_type;
                ValueUnion  value = a_source.m_value;

                a_source.m_type = VT_NULL;
                a_source.m_value.o = 0;

                this->~Value();

                m_type = type;
                m_value = value;
            }

            return *this;
        }

        // TODO Why have this? It destroys the original. Prob should DELETE
        Value& operator=( Value& a_source ) {
            if ( this != &a_source ) {
                ValueType   type = a_source.m_type;
                ValueUnion  value = a_source.m_value;

                a_source.m_type = VT_NULL;
                a_source.m_value.o = 0;

                this->~Value();

                m_type = type;
                m_value = value;
            }

            return *this;
        }

        Value& operator=( bool a_value ) {
            if ( m_type != VT_BOOL ) {
                this->~Value();
                m_type = VT_BOOL;
                m_value.o = 0;
            }

            m_value.b = a_value;

            return *this;
        }

        Value& operator=( double a_value ) {
            if ( m_type != VT_NUMBER ) {
                this->~Value();
                m_type = VT_NUMBER;
                m_value.o = 0;
            }

            m_value.n = a_value;

            return *this;
        }

        Value& operator=( int a_value ) {
            if ( m_type != VT_NUMBER ) {
                this->~Value();
                m_type = VT_NUMBER;
                m_value.o = 0;
            }

            m_value.n = a_value;

            return *this;
        }

        Value& operator=( size_t a_value ) {
            if ( m_type != VT_NUMBER ) {
                this->~Value();
                m_type = VT_NUMBER;
                m_value.o = 0;
            }

            m_value.n = a_value;

            return *this;
        }

        Value& operator=( const std::string& a_value ) {
            if ( m_type != VT_STRING ) {
                this->~Value();
                m_type = VT_STRING;
                m_value.s = new String( a_value );
            }

            *m_value.s = a_value;

            return *this;
        }

        Value& operator=( const char* a_value ) {
            if ( m_type != VT_STRING ) {
                this->~Value();
                m_type = VT_STRING;
                m_value.s = new String( a_value );
            }

            *m_value.s = a_value;

            return *this;
        }

        inline ValueType getType() const {
            return m_type;
        }

        const char* getTypeString() const {
            switch ( m_type ) {
                case VT_NULL: return "NULL";
                case VT_OBJECT: return "OBJECT";
                case VT_ARRAY: return "ARRAY";
                case VT_STRING: return "STRING";
                case VT_NUMBER: return "NUMBER";
                case VT_BOOL: return "BOOL";
                default: return "INVALID";
            }
        }

        inline bool isNull() const {
            return m_type == VT_NULL;
        }

        inline bool isObject() const {
            return m_type == VT_OBJECT;
        }

        inline bool isArray() const {
            return m_type == VT_ARRAY;
        }

        inline bool isString() const {
            return m_type == VT_STRING;
        }

        inline bool isNumber() const {
            return m_type == VT_NUMBER;
        }

        inline bool isBool() const {
            return m_type == VT_BOOL;
        }

        bool asBool() const {
            if ( m_type == VT_BOOL ) {
                return m_value.b;
            } else if ( m_type == VT_NUMBER ) {
                return (bool)m_value.n;
            }

            throw typeConversionError( "boolean" );
        }

        double asNumber() const {
            if ( m_type == VT_NUMBER ) {
                return m_value.n;
            } else if ( m_type == VT_BOOL ) {
                return m_value.b ? 1 : 0;
            }

            throw typeConversionError( "number" );
        }

        std::string& asString() {
            if ( m_type == VT_STRING ) {
                return *m_value.s;
            }

            throw typeConversionError( "string" );
        }

        const std::string& asString() const {
            if ( m_type == VT_STRING ) {
                return *m_value.s;
            }

            throw typeConversionError( "string" );
        }

        // ----- Object & Array Methods -----

        size_t size() const {
            if ( m_type == VT_OBJECT ) {
                return m_value.o->size();
            } else if ( m_type == VT_ARRAY ) {
                return m_value.a->size();
            }

            throw std::runtime_error( "Value::size() requires object or array type" );
        }

        void clear() {
            if ( m_type == VT_OBJECT ) {
                m_value.o->clear();
            } else if ( m_type == VT_ARRAY ) {
                m_value.a->clear();
            } else {
                m_value.o = 0;
            }
        }

        // ----- Object-only Methods -----

        Object& initObject() {
            this->~Value();
            m_type = VT_OBJECT;
            m_value.o = new Object();

            return *m_value.o;
        }

        Object & asObject() {
            if ( m_type != VT_OBJECT ) {
                throw std::runtime_error( "Value is not an object" );
            }

            return *m_value.o;
        }

        const Object & asObject() const {
            if ( m_type != VT_OBJECT ) {
                throw std::runtime_error( "Value is not an object" );
            }

            return *m_value.o;
        }

        // ----- Array-only Methods -----

        Array& initArray() {
            this->~Value();
            m_type = VT_ARRAY;
            m_value.a = new Array();

            return *m_value.a;
        }

        Array& asArray() {
            if ( m_type != VT_ARRAY ) {
                throw std::runtime_error( "Value is not an array" );
            }

            return *m_value.a;
        }

        const Array & asArray() const {
            if ( m_type != VT_ARRAY ) {
                throw std::runtime_error( "Value is not an array" );
            }

            return *m_value.a;
        }

        // ----- To/From String Methods -----

        std::string toString() const {
            std::string buffer;
            buffer.reserve( 4096 );
            toStringRecurse( buffer );

            return buffer;
        }

        inline void fromString( const std::string& a_raw_json ) {
            fromString( a_raw_json.c_str() );
        }

        void fromString( const char* a_raw_json ) {
            if ( m_type != VT_NULL ) {
                this->~Value();
                m_type = VT_NULL;
                m_value.o = 0;
            }

            const char* c = a_raw_json;
            uint8_t         state = PS_SEEK_BEG;

            try {
                while ( *c ) {
                    switch ( state ) {
                        case PS_SEEK_BEG:
                            if ( *c == '{' ) {
                                c = parseObject( *this, c + 1 );
                                state = PS_SEEK_OBJ_END;
                            } else if ( *c == '[' ) {
                                c = parseArray( *this, c + 1 );
                                state = PS_SEEK_ARR_END;
                            } else if ( notWS( *c )) {
                                throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                            }
                            break;
                        case PS_SEEK_OBJ_END:
                            if ( *c == '}' ) {
                                state = PS_SEEK_END;
                            } else if ( notWS( *c )) {
                                throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                            }
                            break;
                        case PS_SEEK_ARR_END:
                            if ( *c == ']' ) {
                                state = PS_SEEK_END;
                            } else if ( notWS( *c )) {
                                throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                            }
                            break;
                        case PS_SEEK_END:
                            if ( notWS( *c )) {
                                throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                            }
                            break;
                    }

                    c++;
                }
            } catch ( ParseError& e ) {
                e.setOffset( a_raw_json );
                throw;
            }
        }


    private:
        std::runtime_error typeConversionError( const std::string & a_type ) const {
            return std::runtime_error( std::string( "Invalid conversion of " ) + getTypeString() + " to " + a_type );
        }

        std::runtime_error typeConversionError( const std::string & a_key, const std::string & a_type ) const {
            return std::runtime_error( std::string( "Invalid conversion of " ) + getTypeString() + " to " + a_type + " for key " + a_key );
        }

        inline bool notWS( char c ) const {
            return !(c == ' ' || c == '\n' || c == '\t' || c == '\r');
        }

        inline bool isDigit( char c ) const {
            return ( c >= '0' && c <= '9' );
        }

        uint8_t toHex( const char* C ) {
            char c = *C;

            if ( c >= '0' && c <= '9' ) {
                return (uint8_t)(c - '0');
            } else if ( c >= 'A' && c <= 'F' ) {
                return (uint8_t)(10 + c - 'A');
            } else if ( c >= 'a' && c <= 'f' ) {
                return (uint8_t)(10 + c - 'a');
            } else {
                throw ParseError( detail::STR_ERR_INVALID_CHAR, C );
            }
        }

        enum ParseState : uint8_t {
            PS_SEEK_BEG,
            PS_SEEK_KEY,
            PS_IN_KEY,
            PS_SEEK_SEP,
            PS_SEEK_VAL,
            PS_IN_VAL_STR,
            PS_IN_VAL_BOOL,
            PS_IN_VAL_NUM,
            PS_NUM_INT,
            PS_NUM_FRAC,
            PS_NUM_EXP,
            PS_SEEK_OBJ_END,
            PS_SEEK_ARR_END,
            PS_SEEK_END,
        };

        ValueType   m_type;

        union ValueUnion {
            Object*     o;
            Array*      a;
            bool        b;
            double      n;
            String*     s;
        } m_value;


        void toStringRecurse( std::string& a_buffer ) const {
            switch ( m_type ) {
                case VT_OBJECT:
                    a_buffer.append( "{" );
                    for ( ObjectIter i = m_value.o->begin(); i != m_value.o->end(); ++i ) {
                        if ( i != m_value.o->begin() ) {
                            a_buffer.append( ",\"" );
                        } else {
                            a_buffer.append( "\"" );
                        }
                        a_buffer.append( i->first );
                        a_buffer.append( "\":" );

                        i->second.toStringRecurse( a_buffer );
                    }
                    a_buffer.append( "}" );
                    break;
                case VT_ARRAY:
                    a_buffer.append( "[" );
                    for ( ArrayIter i = m_value.a->begin(); i != m_value.a->end(); ++i ) {
                        if ( i != m_value.a->begin() ) {
                            a_buffer.append( "," );
                        }
                        i->toStringRecurse( a_buffer );
                    }
                    a_buffer.append( "]" );
                    break;
                case VT_STRING:
                    strToString( a_buffer, *m_value.s );
                    break;
                case VT_NUMBER:
                    numToString( a_buffer, m_value.n );
                    break;
                case VT_BOOL:
                    if ( m_value.b ) {
                        a_buffer.append( "true" );
                    } else {
                        a_buffer.append( "false" );
                    }
                    break;
                case VT_NULL:
                    a_buffer.append( "null" );
                    break;
            }
        }

        inline void strToString( std::string& a_buffer, const std::string& a_value ) const {
            std::string::const_iterator c = a_value.begin();
            std::string::const_iterator a = c;

            a_buffer.append( "\"" );

            for ( c = a_value.begin(); c != a_value.end(); ++c ) {
                if ( *c < 0x20 ) {
                    a_buffer.append( a, c );
                    a = c + 1;

                    switch ( *c ) {
                        case '\b':  a_buffer.append( "\\b" ); break;
                        case '\f':  a_buffer.append( "\\f" ); break;
                        case '\n':  a_buffer.append( "\\n" ); break;
                        case '\r':  a_buffer.append( "\\r" ); break;
                        case '\t':  a_buffer.append( "\\t" ); break;
                    }
                } else if ( *c == '\"' ) {
                    a_buffer.append( a, c );
                    a_buffer.append( "\\\"" );
                    a = c + 1;
                } else if ( *c == '\\' ) {
                    a_buffer.append( a, c );
                    a_buffer.append( "\\\\" );
                    a = c + 1;
                }
            }

            a_buffer.append( a, c );
            a_buffer.append( "\"" );
        }

        inline void numToString( std::string& a_buffer, double a_value ) const {
            size_t sz1 = a_buffer.size();
            a_buffer.resize( sz1 + 50 );
            int sz2 = fpconv_dtoa( a_value, (char*)a_buffer.c_str() + sz1 );
            a_buffer.resize( sz1 + sz2 );
        }

        const char* parseObject( Value& a_parent, const char* start ) {
            // On function entry, c is next char after '{'

            uint8_t         state = PS_SEEK_KEY;
            const char* c = start;
            std::string     key;

            a_parent.m_type = VT_OBJECT;
            a_parent.m_value.o = new Object();

            while ( *c ) {
                switch ( state ) {
                    case PS_SEEK_KEY:
                        if ( *c == '}' ) {
                            return c;
                        } else if ( *c == '"' ) {
                            c = parseString( key, c + 1 );

                            if ( !key.size() ) {
                                throw ParseError( detail::STR_ERR_INVALID_KEY, c );
                            }

                            state = PS_SEEK_SEP;
                        } else if ( notWS( *c ) ) {
                            throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                        }
                        break;
                    case PS_SEEK_SEP:
                        if ( *c == ':' ) {
                            state = PS_SEEK_VAL;
                        } else if ( notWS( *c ) ) {
                            throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                        }
                        break;
                    case PS_SEEK_VAL:
                        if ( notWS( *c ) ) {
                            c = parseValue( (*a_parent.m_value.o)[key], c );
                            state = PS_SEEK_OBJ_END;
                        }
                        break;

                    case PS_SEEK_OBJ_END:
                        if ( *c == ',' ) {
                            state = PS_SEEK_KEY;
                        } else if ( *c == '}' ) {
                            return c;
                        } else if ( notWS( *c ) ) {
                            throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                        }
                        break;
                }

                c++;
            }

            throw ParseError( detail::STR_ERR_UNTERMINATED_OBJECT, start );
        }

        const char* parseArray( Value& a_parent, const char* start ) {
            // On function entry, c is next char after '['
            const char* c = start;
            uint8_t         state = PS_SEEK_VAL;
            Value           value;

            a_parent.m_type = VT_ARRAY;
            a_parent.m_value.a = new Array();
            a_parent.m_value.a->reserve( 20 );

            while ( *c ) {
                switch ( state ) {
                    case PS_SEEK_VAL:
                        if ( *c == ']' ) {
                            return c;
                        } else if ( notWS( *c ) ) {
                            c = parseValue( value, c );
                            a_parent.m_value.a->push_back( std::move( value ) );
                            state = PS_SEEK_SEP;
                        }
                        break;
                    case PS_SEEK_SEP:
                        if ( *c == ',' ) {
                            state = PS_SEEK_VAL;
                        } else if ( *c == ']' ) {
                            return c;
                        } else if ( notWS( *c ) ) {
                            throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                        }
                        break;
                }

                c++;
            }

            throw ParseError( detail::STR_ERR_UNTERMINATED_ARRAY, start );
        }

        inline const char* parseValue( Value& a_value, const char* start ) {
            const char* c = start;

            while ( *c ) {
                switch ( *c ) {
                    case '{':
                        c = parseObject( a_value, c + 1 );
                        return c;
                    case '[':
                        c = parseArray( a_value, c + 1 );
                        return c;
                    case '"':
                        a_value.m_type = VT_STRING;
                        a_value.m_value.s = new String();
                        c = parseString( *a_value.m_value.s, c + 1 );
                        return c;
                    case 't':
                        if ( *(c + 1) == 'r' && *(c + 2) == 'u' && *(c + 3) == 'e' ) {
                            a_value.m_type = VT_BOOL;
                            a_value.m_value.b = true;
                            c += 3;
                            return c;
                        } else {
                            throw ParseError( detail::STR_ERR_INVALID_VALUE, c );
                        }
                        break;
                    case 'f':
                        if ( *(c + 1) == 'a' && *(c + 2) == 'l' && *(c + 3) == 's' && *(c + 4) == 'e' ) {
                            a_value.m_type = VT_BOOL;
                            a_value.m_value.b = false;
                            c += 4;
                            return c;
                        } else {
                            throw ParseError( detail::STR_ERR_INVALID_VALUE, c );
                        }
                        break;
                    case 'n':
                        if ( *(c + 1) == 'u' && *(c + 2) == 'l' && *(c + 3) == 'l' ) {
                            a_value.m_type = VT_NULL;
                            c += 3;
                            return c;
                        } else {
                            throw ParseError( detail::STR_ERR_INVALID_VALUE, c );
                        }
                        break;
                    default:
                        if ( *c == '-' || isDigit( *c ) || *c == '.' ) {
                            a_value.m_type = VT_NUMBER;
                            c = parseNumber( a_value.m_value.n, c );
                            return c;
                        } else if ( notWS( *c ) ) {
                            throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                        }
                        break;
                }

                c++;
            }

            throw ParseError( detail::STR_ERR_UNTERMINATED_VALUE, start );
        }

        inline const char* parseString( std::string& a_value, const char* start ) {
            // On entry, c is next char after "
            const char* c = start;
            const char* a = start;
            uint32_t        utf8;

            a_value.clear();

            while ( *c ) {
                if ( *c == '\\' ) {
                    if ( c != a ) {
                        a_value.append( a, (unsigned int)( c - a ));
                    }

                    switch ( *(c + 1) ) {
                        case 'b':  a_value.append( "\b" ); break;
                        case 'f':  a_value.append( "\f" ); break;
                        case 'n':  a_value.append( "\n" ); break;
                        case 'r':  a_value.append( "\r" ); break;
                        case 't':  a_value.append( "\t" ); break;
                        case '/':  a_value.append( "/" ); break;
                        case '"':  a_value.append( "\"" ); break;
                        case '\\':  a_value.append( "\\" ); break;
                        case 'u':
                            utf8 = ( uint32_t )((toHex( c + 2 ) << 12) | (toHex( c + 3 ) << 8) | (toHex( c + 4 ) << 4) | toHex( c + 5 ));

                            if ( utf8 < 0x80 ) {
                                a_value.append( 1, (char)utf8 );
                            } else if ( utf8 < 0x800 ) {
                                a_value.append( 1, (char)(0xC0 | (utf8 >> 6)) );
                                a_value.append( 1, (char)(0x80 | (utf8 & 0x3F)) );
                            } else if ( utf8 < 0x10000 ) {
                                a_value.append( 1, (char)(0xE0 | (utf8 >> 12)) );
                                a_value.append( 1, (char)(0x80 | ((utf8 >> 6) & 0x3F)) );
                                a_value.append( 1, (char)(0x80 | (utf8 & 0x3F)) );
                            } else if ( utf8 < 0x110000 ) {
                                a_value.append( 1, (char)(0xF0 | (utf8 >> 18)) );
                                a_value.append( 1, (char)(0x80 | ((utf8 >> 12) & 0x3F)) );
                                a_value.append( 1, (char)(0x80 | ((utf8 >> 6) & 0x3F)) );
                                a_value.append( 1, (char)(0x80 | (utf8 & 0x3F)) );
                            } else {
                                throw ParseError( detail::STR_ERR_INVALID_UNICODE, c );
                            }

                            c += 4;
                            break;
                        default:
                            throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                    }

                    c++;
                    a = c + 1;
                } else if ( *c == '"' ) {
                    if ( c != a ) {
                        a_value.append( a, (unsigned int)(c - a));
                    }
                    return c;
                } else if ( *c >= 0 && *c < 0x20 ) {
                    throw ParseError( detail::STR_ERR_INVALID_CHAR, c );
                }

                c++;
            }

            throw ParseError( detail::STR_ERR_UNTERMINATED_VALUE, start );
        }

        inline const char* parseNumber( double& a_value, const char* start ) {
            char* end;
            a_value = strtod( start, &end );

            return end - 1;
        }
    };
}




#include <stdbool.h>
#include <string.h>

#define npowers     87
#define steppowers  8
#define firstpower -348 /* 10 ^ -348 */

#define expmax     -32
#define expmin     -60

typedef struct Fp {
    uint64_t frac;
    int exp;
} Fp;

#define fracmask  0x000FFFFFFFFFFFFFU
#define expmask   0x7FF0000000000000U
#define hiddenbit 0x0010000000000000U
#define signmask  0x8000000000000000U
#define expbias   (1023 + 52)

#define absv(n) ((n) < 0 ? -(n) : (n))
#define minv(a, b) ((a) < (b) ? (a) : (b))

static uint64_t tens[] = {
    10000000000000000000U, 1000000000000000000U, 100000000000000000U,
    10000000000000000U, 1000000000000000U, 100000000000000U,
    10000000000000U, 1000000000000U, 100000000000U,
    10000000000U, 1000000000U, 100000000U,
    10000000U, 1000000U, 100000U,
    10000U, 1000U, 100U,
    10U, 1U
};


static Fp powers_ten[] = {
    { 18054884314459144840U, -1220 }, { 13451937075301367670U, -1193 },
    { 10022474136428063862U, -1166 }, { 14934650266808366570U, -1140 },
    { 11127181549972568877U, -1113 }, { 16580792590934885855U, -1087 },
    { 12353653155963782858U, -1060 }, { 18408377700990114895U, -1034 },
    { 13715310171984221708U, -1007 }, { 10218702384817765436U, -980 },
    { 15227053142812498563U, -954 },  { 11345038669416679861U, -927 },
    { 16905424996341287883U, -901 },  { 12595523146049147757U, -874 },
    { 9384396036005875287U,  -847 },  { 13983839803942852151U, -821 },
    { 10418772551374772303U, -794 },  { 15525180923007089351U, -768 },
    { 11567161174868858868U, -741 },  { 17236413322193710309U, -715 },
    { 12842128665889583758U, -688 },  { 9568131466127621947U,  -661 },
    { 14257626930069360058U, -635 },  { 10622759856335341974U, -608 },
    { 15829145694278690180U, -582 },  { 11793632577567316726U, -555 },
    { 17573882009934360870U, -529 },  { 13093562431584567480U, -502 },
    { 9755464219737475723U,  -475 },  { 14536774485912137811U, -449 },
    { 10830740992659433045U, -422 },  { 16139061738043178685U, -396 },
    { 12024538023802026127U, -369 },  { 17917957937422433684U, -343 },
    { 13349918974505688015U, -316 },  { 9946464728195732843U,  -289 },
    { 14821387422376473014U, -263 },  { 11042794154864902060U, -236 },
    { 16455045573212060422U, -210 },  { 12259964326927110867U, -183 },
    { 18268770466636286478U, -157 },  { 13611294676837538539U, -130 },
    { 10141204801825835212U, -103 },  { 15111572745182864684U, -77 },
    { 11258999068426240000U, -50 },   { 16777216000000000000U, -24 },
    { 12500000000000000000U,   3 },   { 9313225746154785156U,   30 },
    { 13877787807814456755U,  56 },   { 10339757656912845936U,  83 },
    { 15407439555097886824U, 109 },   { 11479437019748901445U, 136 },
    { 17105694144590052135U, 162 },   { 12744735289059618216U, 189 },
    { 9495567745759798747U,  216 },   { 14149498560666738074U, 242 },
    { 10542197943230523224U, 269 },   { 15709099088952724970U, 295 },
    { 11704190886730495818U, 322 },   { 17440603504673385349U, 348 },
    { 12994262207056124023U, 375 },   { 9681479787123295682U,  402 },
    { 14426529090290212157U, 428 },   { 10748601772107342003U, 455 },
    { 16016664761464807395U, 481 },   { 11933345169920330789U, 508 },
    { 17782069995880619868U, 534 },   { 13248674568444952270U, 561 },
    { 9871031767461413346U,  588 },   { 14708983551653345445U, 614 },
    { 10959046745042015199U, 641 },   { 16330252207878254650U, 667 },
    { 12166986024289022870U, 694 },   { 18130221999122236476U, 720 },
    { 13508068024458167312U, 747 },   { 10064294952495520794U, 774 },
    { 14996968138956309548U, 800 },   { 11173611982879273257U, 827 },
    { 16649979327439178909U, 853 },   { 12405201291620119593U, 880 },
    { 9242595204427927429U,  907 },   { 13772540099066387757U, 933 },
    { 10261342003245940623U, 960 },   { 15290591125556738113U, 986 },
    { 11392378155556871081U, 1013 },  { 16975966327722178521U, 1039 },
    { 12648080533535911531U, 1066 }
};

static Fp find_cachedpow10(int exp, int* k)
{
    const double one_log_ten = 0.30102999566398114;

    int approx = -(exp + npowers) * one_log_ten;
    int idx = (approx - firstpower) / steppowers;

    while(1) {
        int current = exp + powers_ten[idx].exp + 64;

        if(current < expmin) {
            idx++;
            continue;
        }

        if(current > expmax) {
            idx--;
            continue;
        }

        *k = (firstpower + idx * steppowers);

        return powers_ten[idx];
    }
}


static inline uint64_t get_dbits(double d)
{
    union {
        double   dbl;
        uint64_t i;
    } dbl_bits = { d };

    return dbl_bits.i;
}

static Fp build_fp(double d)
{
    uint64_t bits = get_dbits(d);

    Fp fp;
    fp.frac = bits & fracmask;
    fp.exp = (bits & expmask) >> 52;

    if(fp.exp) {
        fp.frac += hiddenbit;
        fp.exp -= expbias;

    } else {
        fp.exp = -expbias + 1;
    }

    return fp;
}

static void normalize(Fp* fp)
{
    while ((fp->frac & hiddenbit) == 0) {
        fp->frac <<= 1;
        fp->exp--;
    }

    int shift = 64 - 52 - 1;
    fp->frac <<= shift;
    fp->exp -= shift;
}

static void get_normalized_boundaries(Fp* fp, Fp* lower, Fp* upper)
{
    upper->frac = (fp->frac << 1) + 1;
    upper->exp  = fp->exp - 1;

    while ((upper->frac & (hiddenbit << 1)) == 0) {
        upper->frac <<= 1;
        upper->exp--;
    }

    int u_shift = 64 - 52 - 2;

    upper->frac <<= u_shift;
    upper->exp = upper->exp - u_shift;


    int l_shift = fp->frac == hiddenbit ? 2 : 1;

    lower->frac = (fp->frac << l_shift) - 1;
    lower->exp = fp->exp - l_shift;


    lower->frac <<= lower->exp - upper->exp;
    lower->exp = upper->exp;
}

static Fp multiply(Fp* a, Fp* b)
{
    const uint64_t lomask = 0x00000000FFFFFFFF;

    uint64_t ah_bl = (a->frac >> 32)    * (b->frac & lomask);
    uint64_t al_bh = (a->frac & lomask) * (b->frac >> 32);
    uint64_t al_bl = (a->frac & lomask) * (b->frac & lomask);
    uint64_t ah_bh = (a->frac >> 32)    * (b->frac >> 32);

    uint64_t tmp = (ah_bl & lomask) + (al_bh & lomask) + (al_bl >> 32);
    /* round up */
    tmp += 1U << 31;

    Fp fp = {
        ah_bh + (ah_bl >> 32) + (al_bh >> 32) + (tmp >> 32),
        a->exp + b->exp + 64
    };

    return fp;
}

static void round_digit(char* digits, int ndigits, uint64_t delta, uint64_t rem, uint64_t kappa, uint64_t frac)
{
    while (rem < frac && delta - rem >= kappa &&
           (rem + kappa < frac || frac - rem > rem + kappa - frac)) {

        digits[ndigits - 1]--;
        rem += kappa;
    }
}

static int generate_digits(Fp* fp, Fp* upper, Fp* lower, char* digits, int* K)
{
    uint64_t wfrac = upper->frac - fp->frac;
    uint64_t delta = upper->frac - lower->frac;

    Fp one;
    one.frac = 1ULL << -upper->exp;
    one.exp  = upper->exp;

    uint64_t part1 = upper->frac >> -one.exp;
    uint64_t part2 = upper->frac & (one.frac - 1);

    int idx = 0, kappa = 10;
    uint64_t* divp;
    /* 1000000000 */
    for(divp = tens + 10; kappa > 0; divp++) {

        uint64_t div = *divp;
        unsigned digit = part1 / div;

        if (digit || idx) {
            digits[idx++] = digit + '0';
        }

        part1 -= digit * div;
        kappa--;

        uint64_t tmp = (part1 <<-one.exp) + part2;
        if (tmp <= delta) {
            *K += kappa;
            round_digit(digits, idx, delta, tmp, div << -one.exp, wfrac);

            return idx;
        }
    }

    /* 10 */
    uint64_t* unit = tens + 18;

    while(true) {
        part2 *= 10;
        delta *= 10;
        kappa--;

        unsigned digit = part2 >> -one.exp;
        if (digit || idx) {
            digits[idx++] = digit + '0';
        }

        part2 &= one.frac - 1;
        if (part2 < delta) {
            *K += kappa;
            round_digit(digits, idx, delta, part2, one.frac, wfrac * *unit);

            return idx;
        }

        unit--;
    }
}

static int grisu2(double d, char* digits, int* K)
{
    Fp w = build_fp(d);

    Fp lower, upper;
    get_normalized_boundaries(&w, &lower, &upper);

    normalize(&w);

    int k;
    Fp cp = find_cachedpow10(upper.exp, &k);

    w     = multiply(&w,     &cp);
    upper = multiply(&upper, &cp);
    lower = multiply(&lower, &cp);

    lower.frac++;
    upper.frac--;

    *K = -k;

    return generate_digits(&w, &upper, &lower, digits, K);
}

static int emit_digits(char* digits, int ndigits, char* dest, int K, bool neg)
{
    int exp = absv(K + ndigits - 1);

    /* write plain integer */
    if(K >= 0 && (exp < (ndigits + 7))) {
        memcpy(dest, digits, ndigits);
        memset(dest + ndigits, '0', K);

        return ndigits + K;
    }

    /* write decimal w/o scientific notation */
    if(K < 0 && (K > -7 || exp < 4)) {
        int offset = ndigits - absv(K);
        /* fp < 1.0 -> write leading zero */
        if(offset <= 0) {
            offset = -offset;
            dest[0] = '0';
            dest[1] = '.';
            memset(dest + 2, '0', offset);
            memcpy(dest + offset + 2, digits, ndigits);

            return ndigits + 2 + offset;

        /* fp > 1.0 */
        } else {
            memcpy(dest, digits, offset);
            dest[offset] = '.';
            memcpy(dest + offset + 1, digits + offset, ndigits - offset);

            return ndigits + 1;
        }
    }

    /* write decimal w/ scientific notation */
    ndigits = minv(ndigits, 18 - neg);

    int idx = 0;
    dest[idx++] = digits[0];

    if(ndigits > 1) {
        dest[idx++] = '.';
        memcpy(dest + idx, digits + 1, ndigits - 1);
        idx += ndigits - 1;
    }

    dest[idx++] = 'e';

    char sign = K + ndigits - 1 < 0 ? '-' : '+';
    dest[idx++] = sign;

    int cent = 0;

    if(exp > 99) {
        cent = exp / 100;
        dest[idx++] = cent + '0';
        exp -= cent * 100;
    }
    if(exp > 9) {
        int dec = exp / 10;
        dest[idx++] = dec + '0';
        exp -= dec * 10;

    } else if(cent) {
        dest[idx++] = '0';
    }

    dest[idx++] = exp % 10 + '0';

    return idx;
}

static int filter_special(double fp, char* dest)
{
    if(fp == 0.0) {
        dest[0] = '0';
        return 1;
    }

    uint64_t bits = get_dbits(fp);

    bool nan = (bits & expmask) == expmask;

    if(!nan) {
        return 0;
    }

    if(bits & fracmask) {
        dest[0] = 'n'; dest[1] = 'a'; dest[2] = 'n';

    } else {
        dest[0] = 'i'; dest[1] = 'n'; dest[2] = 'f';
    }

    return 3;
}

int fpconv_dtoa(double d, char dest[24])
{
    char digits[18];

    int str_len = 0;
    bool neg = false;

    if(get_dbits(d) & signmask) {
        dest[0] = '-';
        str_len++;
        neg = true;
    }

    int spec = filter_special(d, dest + str_len);

    if(spec) {
        return str_len + spec;
    }

    int K = 0;
    int ndigits = grisu2(d, digits, &K);

    str_len += emit_digits(digits, ndigits, dest + str_len, K, neg);

    return str_len;
}


#endif
