#include "stdafx.h"
#include "ParseJSON.h"

#include <string>

namespace Morrin::JSON
{
inline  bool IsWhiteSpace( std::string_view::value_type value )
{
	//	ホワイトスペースとして規定されている。isspace とはちょっと違うので定義を厳格化
	switch( value )
	{
	case ' ':
	case '\t':
	case '\r':
	case '\n':
		return true;
	}
	return false;
}
inline std::string_view::size_type SkipWhiteSpace( const std::string_view& rowData, std::string_view::size_type offset )
{
	while( offset < rowData.length() && IsWhiteSpace( rowData[offset] ) )
	{
		++offset;
	}
	return offset;
}
static bool ParseElement( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseMember( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseValues( std::function<bool( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )>&& parseProc,
						 ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );

static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseStringValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );
static bool ParseLiteralValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, const std::string_view& checkValue, NotificationId type );
static bool ParseNumberValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset );

static std::string_view ExtractStringValue( const std::string_view& rowData, std::string_view::size_type& offset );

//	エレメントのパース
static bool ParseElement( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( offset < rowData.length() );
	offset = SkipWhiteSpace( rowData, offset );
	//	ホワイトスペースをスキップしてデータが残っている場合のみ処理する(残っていないこともあり得る)
	if( offset < rowData.length() )
	{
		switch( rowData[offset] )
		{
		case '{':	//	object
			if( !ParseObject( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case '[':	//	array
			if( !ParseArray( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case '"':	//	string
			//	オブジェクトの途中ではないので、オブジェクトキーではない
			if( !ParseStringValue( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		case 't':	//	リテラルチェック == true
			if( !ParseLiteralValue( proc, rowData, offset, "true", NotificationId::True ) )
			{
				return false;
			}
			break;
		case 'f':	//	リテラルチェック == false
			if( !ParseLiteralValue( proc, rowData, offset, "false", NotificationId::False ) )
			{
				return false;
			}
			break;
		case 'n':	//	リテラルチェック == null
			if( !ParseLiteralValue( proc, rowData, offset, "null", NotificationId::Null ) )
			{
				return false;
			}
			break;
		default:	//	数値のはず。一応アサーションくらいは入れて自衛はする
			if( !ParseNumberValue( proc, rowData, offset ) )
			{
				return false;
			}
			break;
		}
		//	エレメントに後続するホワイトスペースはスキップする
		offset = SkipWhiteSpace( rowData, offset );
	}
	return true;
}
static bool ParseMember( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	offset = SkipWhiteSpace( rowData, offset );	//	'{' の次から空白部分を除外
	_ASSERTE( rowData[offset] == '"' );
	auto key = ExtractStringValue( rowData, offset );
	if( !proc( NotificationId::Key, key ) )
	{
		return false;
	}
	offset = SkipWhiteSpace( rowData, offset );
	_ASSERTE( rowData[offset] == ':' );
	//	データがおかしい場合はそこで終了でいい
	if( rowData[offset++] != ':' )
	{
		return false;
	}
	if( !ParseElement( proc, rowData, offset ) )
	{
		return false;
	}
	offset = SkipWhiteSpace( rowData, offset );
	return true;
}
static bool ParseValues( std::function<bool( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )>&& parseProc,
						 ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	while( offset < rowData.length() )
	{
		if( !parseProc( proc, rowData, offset ) )
		{
			return false;
		}
		//	Elements/Members は ',' が後続する場合、もう一回 Element/Member が来る
		if( offset < rowData.length() && rowData[offset] == ',' )
		{
			//	区切りを通知する
			if( !proc( NotificationId::Comma, rowData.substr( offset++, 1 ) ) )
			{
				return false;	//	キャンセルされた
			}
		}
		else
		{
			break;
		}
	}
	//	処理を正常に終了したので成功終了する	//	Elements の終わりは何も言わない
	return true;
}
static bool ParseObject( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '{' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( !proc( NotificationId::StartObject, rowData.substr( offset++, 1 ) ) )
	{
		return false;
	}
	if( !ParseValues( ParseMember, proc, rowData, offset ) )
	{
		return false;
	}
	_ASSERTE( rowData[offset] == '}' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( rowData[offset] != '}' )
	{
		return false;
	}
	return proc( NotificationId::EndObject, rowData.substr( offset++, 1 ) );
}
static bool ParseArray( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '[' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( !proc( NotificationId::StartArray, rowData.substr( offset++, 1 ) ) )
	{
		return false;
	}
	if( !ParseValues( ParseElement, proc, rowData, offset ) )
	{
		return false;
	}
	offset = SkipWhiteSpace( rowData, offset );
	_ASSERTE( rowData[offset] == ']' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( rowData[offset] != ']' )
	{
		return false;
	}
	return proc( NotificationId::EndArray, rowData.substr( offset++, 1 ) );
}
static bool ParseStringValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '"' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	auto value = ExtractStringValue( rowData, offset );	//	末尾の '"' の次まで移動。実際の値は参照しないけど、ここで切り出しているので長さは見れる
	return proc( NotificationId::String, value );
}
static bool ParseLiteralValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset, const std::string_view& checkValue, NotificationId type )
{
	if( rowData.compare( offset, checkValue.length(), checkValue ) == 0 )
	{
		offset += checkValue.length();
		return proc( type, checkValue );
	}
	return false;
}
static bool ParseNumberValue( ParseCallBack& proc, const std::string_view& rowData, std::string_view::size_type& offset )
{
	//	数値データは、数字もしくは、- で始まる。
	_ASSERTE( isdigit( rowData[offset] ) || rowData[offset] == '-' );	//	数値または、- で始まる
	//	整数ばかりとは限らないので、数値データは文字列のまま渡す。理想は、int/double 当たりなんだろうけど。。。
	auto start = rowData.data()+offset;
	char* end = nullptr;
	auto doubleValue = strtod( start, &end );
	auto length = end-start;
	auto value = rowData.substr( offset, length );
	offset += length;
	return proc( NotificationId::Digit, value );
}
static std::string_view ExtractStringValue( const std::string_view& rowData, std::string_view::size_type& offset )
{
	_ASSERTE( rowData[offset] == '"' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	++offset;	//	'"' をスキップ
	auto start = offset;
	//	テキストの終端部分までステップする
	while( offset < rowData.length() && rowData[offset] != '"' )
	{
		//	エスケープ記号
		if( rowData[offset++] == '\\' )
		{
			switch( rowData[offset++] )
			{
			case 'u':
				//	規定で4文字だけど念のため。厳格チェックしない場合は+4でスキップしてよい
				for( int cnt = 0 ; cnt < 4 ; ++cnt, ++offset )
				{
					//	本当は途中で終わるとか言語道断レベルよね。。。
					if( !isxdigit( rowData[offset] ) )
					{
						return false;
					}
				}
				break;
			//	厳格チェックしない場合は、このあたりはコメントアウトでよい
			case '"':
			case '\\':
			case '/':
			case 'b':
			case 'n':
			case 'r':
			case 't':
				break;
			default:
				return false;
			}
		}
	}
	_ASSERTE( rowData[offset] == '"' );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	auto result = rowData.substr( start, offset-start );
	++offset;
	return result;
}
///////////////////////////////////////////////////////////////////////////////
//	global Functions.
bool APIENTRY ParseJSON( const std::string_view& rowData, ParseCallBack& proc )
{
	std::string_view::size_type offset = 0;
	//	開始を通知(空文字列)
	if( !proc( NotificationId::StartParse, rowData ) )
	{
		return false;
	}
	while( offset < rowData.length() )
	{
		offset = SkipWhiteSpace( rowData, offset );
		//	ホワイトスペースを除外したらなくなったらその時点で終わる(Element の前後にWhiteSpaceがある)
		if( offset >= rowData.length() )
		{
			break;
		}
		//	何らかの理由でパースが失敗した(通常は、コールバックが停止を返した)ら、処理は終了
		if( !ParseElement( proc, rowData, offset ) )
		{
			break;
		}
	}
	//	終了を通知。offset の位置で判定可能なので必要があればそれを見ることとする
	return proc( NotificationId::EndParse, rowData.substr( offset ) );	//	なにか残ってるという想定で全部送り込んでやる(さらにネスト的な処理があってもよいという判定)
}
//	テキストのエスケープ処理を解決する
std::string APIENTRY ExtractEscapeString( const std::string_view& escapeValue )
{
	std::string_view::size_type pos = 0;
	std::string resultU8Str;
	auto prevPos = pos;
	while( pos < escapeValue.length() )
	{
		//	エスケープ記号
		if( escapeValue[pos] == '\\' )
		{
			if( prevPos != std::string_view::npos )
			{
				resultU8Str += escapeValue.substr( prevPos, pos-prevPos );
			}
			prevPos = std::string_view::npos;	//	エスケープ文字内部なのでリセット
			++pos;	//	無視
			switch( escapeValue[pos] )
			{
			case 'u':
				//	規定で4文字だけど念のため。厳格チェックしない場合は+4でスキップしてよい
				{
					wchar_t ch = 0;
					for( int cnt = 0 ; cnt < 4 && isxdigit( escapeValue[pos] ) ; ++cnt, ++pos )
					{
						if( isdigit( escapeValue[pos] ) )
						{
							ch = (ch << 4) + (escapeValue[pos] - '0');
						}
						else
						{
							ch = (ch<<4) + ( (toupper( escapeValue[pos] ) - 'A') + 0xA);
						}
					}
					char utf8str[4+1];	//	UTF8は最大4文字に分解される
					WideCharToMultiByte( CP_UTF8, 0, &ch, 1, utf8str, 4+1, nullptr, nullptr );
					resultU8Str += utf8str;
				}
				break;
			//	厳格チェックしない場合は、このあたりはコメントアウトでよい
			case '"':
			case '\\':
			case '/':
			case 'b':
			case 'n':
			case 'r':
			case 't':
				resultU8Str += escapeValue[pos];
				++pos;
				break;
			default:
				return false;
			}
		}
		else
		{
			if( prevPos == std::string_view::npos )
			{
				prevPos = pos;
			}
			++pos;
		}
	}
	if( prevPos != std::string_view::npos )
	{
		resultU8Str += escapeValue.substr( prevPos, pos-prevPos );
	}
	return resultU8Str;
}
}//	namespace Morrin::JSON
