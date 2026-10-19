using System.Buffers.Binary;
using System.IO;
using System.Text;

namespace StuntmanLauncher;

public sealed record XisoEntry(string Path, bool IsDirectory, uint Sector, uint Size);

// Reads Xbox 360 disc images (XDVDFS, "XISO"). The game partition starts at a
// fixed offset that depends on the disc format; its volume descriptor sits 32
// sectors in and points to the root directory. A directory is a table of
// entries arranged as a binary tree (left/right child offsets in dwords).
public sealed class XisoReader : IDisposable
{
    private const int SectorSize = 2048;
    private static readonly byte[] Magic = Encoding.ASCII.GetBytes("MICROSOFT*XBOX*MEDIA");

    // Plain XISO, XGD3, XGD2, XGD1
    private static readonly long[] PartitionOffsets = { 0, 0x2080000, 0xFD90000, 0x18300000 };

    private readonly FileStream file_;
    private readonly long partition_;

    public List<XisoEntry> Entries { get; } = new();

    public XisoReader(string path)
    {
        file_ = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1 << 20);
        partition_ = PartitionOffsets.FirstOrDefault(HasMagic, -1);
        if (partition_ < 0)
        {
            file_.Dispose();
            throw new InvalidDataException("not an Xbox disc image");
        }

        var descriptor = Read(partition_ + 32L * SectorSize, 28);
        uint rootSector = BinaryPrimitives.ReadUInt32LittleEndian(descriptor.AsSpan(20));
        uint rootSize = BinaryPrimitives.ReadUInt32LittleEndian(descriptor.AsSpan(24));
        ReadDirectory("", rootSector, rootSize);
    }

    private bool HasMagic(long offset)
    {
        if (offset + 32L * SectorSize + Magic.Length > file_.Length) return false;
        return Read(offset + 32L * SectorSize, Magic.Length).AsSpan().SequenceEqual(Magic);
    }

    private byte[] Read(long offset, int count)
    {
        var buffer = new byte[count];
        file_.Position = offset;
        file_.ReadExactly(buffer);
        return buffer;
    }

    private void ReadDirectory(string parent, uint sector, uint size)
    {
        if (size == 0) return;
        var table = Read(partition_ + (long)sector * SectorSize, (int)size);
        var pending = new Stack<int>();
        var seen = new HashSet<int>();
        pending.Push(0);
        while (pending.Count > 0)
        {
            int o = pending.Pop();
            if (o + 14 > table.Length || !seen.Add(o)) continue;
            ushort left = BinaryPrimitives.ReadUInt16LittleEndian(table.AsSpan(o));
            if (left == 0xFFFF) continue;  // padding
            ushort right = BinaryPrimitives.ReadUInt16LittleEndian(table.AsSpan(o + 2));
            uint start = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(o + 4));
            uint length = BinaryPrimitives.ReadUInt32LittleEndian(table.AsSpan(o + 8));
            byte attributes = table[o + 12];
            int nameLength = table[o + 13];
            if (o + 14 + nameLength > table.Length) continue;
            string name = Encoding.ASCII.GetString(table, o + 14, nameLength);

            if (left != 0) pending.Push(left * 4);
            if (right != 0) pending.Push(right * 4);
            if (name.Length == 0 || name.Contains("..") || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
                continue;

            string path = parent.Length == 0 ? name : parent + "\\" + name;
            bool isDir = (attributes & 0x10) != 0;
            Entries.Add(new XisoEntry(path, isDir, start, length));
            if (isDir) ReadDirectory(path, start, length);
        }
    }

    public XisoEntry? Find(string path) =>
        Entries.FirstOrDefault(e => string.Equals(e.Path, path, StringComparison.OrdinalIgnoreCase));

    public byte[] ReadStart(XisoEntry entry, int count) =>
        Read(partition_ + (long)entry.Sector * SectorSize, (int)Math.Min(count, entry.Size));

    public long TotalFileBytes => Entries.Where(e => !e.IsDirectory).Sum(e => (long)e.Size);

    // Writes every file under target. Files already there with the right size
    // are kept, so an interrupted extraction resumes. progress gets bytes done.
    public void ExtractAll(string target, IProgress<long>? progress, CancellationToken cancel)
    {
        var buffer = new byte[1 << 20];
        long done = 0;
        foreach (var entry in Entries)
        {
            cancel.ThrowIfCancellationRequested();
            var path = Path.Combine(target, entry.Path);
            if (entry.IsDirectory)
            {
                Directory.CreateDirectory(path);
                continue;
            }
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            var existing = new FileInfo(path);
            if (existing.Exists && existing.Length == entry.Size)
            {
                done += entry.Size;
                progress?.Report(done);
                continue;
            }

            file_.Position = partition_ + (long)entry.Sector * SectorSize;
            using var output = new FileStream(path + ".part", FileMode.Create, FileAccess.Write, FileShare.None, 1 << 20);
            long left = entry.Size;
            while (left > 0)
            {
                cancel.ThrowIfCancellationRequested();
                int n = file_.Read(buffer, 0, (int)Math.Min(buffer.Length, left));
                if (n <= 0) throw new EndOfStreamException($"disc image ends inside {entry.Path}");
                output.Write(buffer, 0, n);
                left -= n;
                done += n;
                progress?.Report(done);
            }
            output.Dispose();
            File.Move(path + ".part", path, overwrite: true);
        }
    }

    // Title ID from an XEX2 header: the execution info optional header
    // (key 0x00040006) holds media ID, versions, then the title ID.
    public static uint? XexTitleId(byte[] header)
    {
        if (header.Length < 24 || Encoding.ASCII.GetString(header, 0, 4) != "XEX2") return null;
        uint count = BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan(0x14));
        for (int i = 0; i < count && 0x18 + i * 8 + 8 <= header.Length; i++)
        {
            int at = 0x18 + i * 8;
            uint key = BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan(at));
            uint value = BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan(at + 4));
            if (key == 0x00040006 && value + 16 <= header.Length)
                return BinaryPrimitives.ReadUInt32BigEndian(header.AsSpan((int)value + 12));
        }
        return null;
    }

    public void Dispose() => file_.Dispose();
}
