using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using IjwFramework.Collections;
using System.Runtime.InteropServices;
using System.Linq;
using IjwFramework.Types;

namespace ContentHeaderGen
{
	class Program
	{
		static void Main(string[] args)
		{
			var ms = new MemoryStream();
			var blob = new Cache<byte[], Pair<int,int>>(
				x =>
				{
					ms.Write(x, 0, x.Length);
					return new Pair<int, int>( (int)ms.Length - x.Length, x.Length );
				});

			var enc = Encoding.ASCII;
			var namecache = new Cache<string, byte[]>(x => enc.GetBytes(x));

			string dir = args.DefaultIfEmpty(Environment.CurrentDirectory).First();

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

				foreach (var x in new Pair<int, int>[] {  
					blob[namecache[Path.GetFileName(file)]],
					blob[namecache[mime]],
					blob[content] } )
				{
					writer.Write( x.First );
					writer.Write( x.Second ); 
				}
			}

			writer.Write((int)0);
			writer.Write(ms.ToArray());
			writer.Close();
		}

		[DllImport("urlmon.dll", CharSet=CharSet.Unicode)]
		static extern int FindMimeFromData(IntPtr bindContext, string url, IntPtr buf, int bufLen, IntPtr proposedMime, 
			int flags, out string mime, int reserved);
	}
}
