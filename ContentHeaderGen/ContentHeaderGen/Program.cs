using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using IjwFramework.Collections;
using System.Runtime.InteropServices;

namespace ContentHeaderGen
{
	class Program
	{
		static void Main(string[] args)
		{
			var ms = new MemoryStream();
			var blob = new Cache<byte[], OffsetLengthPair>(
				x =>
				{
					ms.Write(x, 0, x.Length);
					return new OffsetLengthPair() { offset = (int)ms.Length - x.Length, length = x.Length };
				});

			var enc = Encoding.ASCII;
			var namecache = new Cache<string, byte[]>(x => enc.GetBytes(x));

			string dir;
			if (args.Length == 0)
				dir = Environment.CurrentDirectory;
			else
				dir = args[0];

			var files = new List<string>();
			files.AddRange(Directory.GetFiles(dir));

			var writer = new BinaryWriter(File.OpenWrite("content.blob"));

			foreach (var file in files)
			{
				Console.WriteLine(file);
				var content = File.ReadAllBytes(file);
				string mime;
				var result = FindMimeFromData(IntPtr.Zero, file, IntPtr.Zero, 0, IntPtr.Zero, 1, out mime, 0);

				mime = mime ?? "application/octet-stream";
				Console.WriteLine(result + " " + mime);

				var fPair = blob[namecache[Path.GetFileName(file)]];
				var mPair = blob[namecache[mime]];
				var cPair = blob[content];

				writer.Write(fPair.offset);
				writer.Write(fPair.length);
				writer.Write(mPair.offset);
				writer.Write(mPair.length);
				writer.Write(cPair.offset);
				writer.Write(cPair.length);
			}
			writer.Write((int)0);
			writer.Write(ms.ToArray());
			writer.Close();
		}

		struct OffsetLengthPair
		{
			public int offset, length;
		}

		[DllImport("urlmon.dll", CharSet=CharSet.Unicode)]
		static extern int FindMimeFromData(IntPtr bindContext, string url, IntPtr buf, int bufLen, IntPtr proposedMime, 
			int flags, out string mime, int reserved);
	}
}
