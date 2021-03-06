﻿package
{
	import flash.utils.*;
	import flash.system.Capabilities;

	class MyUtils 
	{
		static var _bArr:ByteArray;
		
		// converts Vector.<uint> to ByteArray
		static function ToByteArray(v:Vector.<uint>, offs:uint, len:uint):ByteArray
		{
			if (_bArr == null) _bArr = new ByteArray();
			_bArr.length = len*4;
			_bArr.position = 0;
			_bArr.endian = Endian.LITTLE_ENDIAN;
			
			len += offs;
			for(; offs < len; offs++) _bArr.writeUnsignedInt(v[offs]);
			
			return _bArr;
		}		
		
		// compares sequence of uints with string 
		static function IsEqual(v:Vector.<uint>, s:String, offs:uint = 0):Boolean
		{			
			_bArr = ToByteArray(v, 0, v.length);
			_bArr.position = offs;
			for(var i:int=2; i >= 0; i--)
				if (_bArr[i] == 0) { _bArr.position = i+1 + offs; break; }

			return _bArr.readUTFBytes(s.length).toUpperCase() == s;
		}
		
		//
		static function ToStringV(v:Vector.<uint>, offs:uint, len:uint):String
		{
			var str:String = "", c:int;
			
			len += offs;
			for(; offs < len; offs++, c++) {
				str += v[offs].toString(16) + ",";
				if (c == 7) { str += "<br>"; c = -1; }
			}
			
			return str;
		}
		
		static function isWin():Boolean
		{
			return Capabilities.version.toUpperCase().search("WIN") >= 0;
		}
		
		static function isMac():Boolean
		{
			return Capabilities.version.toUpperCase().search("MAC") >= 0;
		}	
	}	
}