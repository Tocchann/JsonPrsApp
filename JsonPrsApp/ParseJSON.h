//	速度優先型JSONパーサー C++17版

//	コンセプト
//	目指せ最速！だからコピーしない！
//	できるだけ型は既定のものを使う！
//	JSONデータの書式の規定は http://www.json.org/json-ja.html を参照(2018/11/05時点)

//	VC++以外ではビルドしてない！

#pragma once

#include <string_view>
#include <functional>
#include <utility>

namespace Morrin::JSON
{
enum NotificationId
{
	StartParse,		//	パース開始(これからデータが送られてくる)
	EndParse,		//	パース終了(これ以上データが送られてくることはない)
	//	複合型の開始と終了(いわゆるマーカー)
	StartObject,	//	'{'
	EndObject,		//	'}'
	StartArray,		//	'['
	EndArray,		//	']'
	//	配列要素のセパレータ
	Comma,
	//	単一型の通知
	Key,			//	オブジェクトのキー(実際は文字列)
	String,			//	文字列データ
	Digit,			//	数値データ
	True,			//	true
	False,			//	false
	Null,			//	null
};

//	パース結果のコールバック受け取りメソッド( bool func( Morrin::JSON::NodeType nodeType, const std::string_view& node ) )
using ParseCallBack = std::function<bool( NotificationId id, const std::string_view& value )> const;

//	JSONパースエンジン本体
bool APIENTRY ParseJSON( const std::string_view& rowData, ParseCallBack& proc );
std::string APIENTRY ExtractEscapeString( const std::string_view& escapeValue );

//	JSONデータの汎用型クラスを作る？として。。。pair<key,value> valueを std::any にするか、variant<int,double,u8string,vector<int,double,u8string>>にするか。。。かなぁ？

}//	Morin::JSON
