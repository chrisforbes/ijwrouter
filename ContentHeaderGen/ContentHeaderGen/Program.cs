using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using IjwFramework.Collections;
using IjwFramework.Types;

namespace ContentHeaderGen
{
	[Flags]
	enum Options
	{
		None = 0,
		Gzip = 1
	}

	class Program
	{
		static Dictionary<string, Options> ParseOptions(string optionsFilePath)
		{
			var options = new Dictionary<string, Options>();
			var lines = File.ReadAllLines(optionsFilePath);
			Regex pattern = new Regex(@"([^=]+)=\s*(\w+)");
			foreach (string line in lines)
			{
				var m = pattern.Match(line);
				if (!m.Success)
					continue;
				Options op = Options.None;
				switch (m.Groups[2].Value)
				{
					case "gzip":
						op |= Options.Gzip;
						break;
					default:
						op = Options.None;
						break;
				}
				options.Add(m.Groups[1].Value.Trim(), op);
			}

			foreach (string k in options.Keys)
				Console.WriteLine(k);

			return options;
		}

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

			Dictionary<string, Options> options = null;
			string optionsFile = Path.Combine(dir, "options.txt");
			if (File.Exists(optionsFile))
				options = ParseOptions(optionsFile);

			var files = new List<string>();
			files.AddRange(Directory.GetFiles(dir)); 

			var writer = new BinaryWriter(File.OpenWrite("content.blob"));

			foreach (var file in files)
			{
				if (Path.GetFileName(file) == "options.txt")
					continue;

				Console.WriteLine(file);
				var content = File.ReadAllBytes(file);
				string mime;
				var result = FindMimeFromData(IntPtr.Zero, file, IntPtr.Zero, 0, IntPtr.Zero, 1, out mime, 0);

				mime = mime ?? "application/octet-stream";

				Options o;
				if (options != null && options.TryGetValue(Path.GetFileName(file), out o))
				{
					if ((o & Options.Gzip) != 0)
						using (var compressedStream = new MemoryStream())
						{
							var gzipStream = new GZipStream(compressedStream, CompressionMode.Compress, true);
							gzipStream.Write(content, 0, content.Length);
							gzipStream.Flush();
							gzipStream.Close();
							mime = "application/x-gzip";
							content = compressedStream.ToArray();
						}
				}

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
