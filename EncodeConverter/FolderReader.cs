namespace EncodeConverter
{
    public struct FILE_INFO
    {
        public string mDirectory;
        public string mFileName;
    }

    public class FolderReader
    {
        private static readonly FolderReader mReader = new();
        private readonly List<FILE_INFO> mFiles = new();
        private string mSearchPath = "";

        public static FolderReader Instance { get { return mReader; } }
        public List<FILE_INFO> Files { get { return mFiles; } }
        public string SearchPath { get { return mSearchPath; } }

        private FolderReader() { }

        public void GetFilesInFolder(string _path, string[] _patterns,
            bool _checkChildFolder)
        {
            mSearchPath = _path;
            if (mSearchPath[^1] != '\\') { mSearchPath += '\\'; }
            mFiles.Clear();
            DirectoryInfo dir = new(_path);
            SearchOption searchOption = _checkChildFolder ?
                SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;

            foreach (string pattern in _patterns)
            {
                IEnumerable<FileInfo> result = dir.EnumerateFiles(
                    pattern, searchOption);
                foreach (FileInfo file in result)
                {
                    if (file.DirectoryName != null && file.Name != null)
                    {
                        FILE_INFO info = new();
                        Uri filePath = new(file.DirectoryName);
                        string absolute = Path.GetFullPath(mSearchPath);
                        Uri searchPath = new(absolute);
                        info.mDirectory = searchPath.MakeRelativeUri(
                            filePath).ToString();
                        info.mDirectory = $"{_path}\\{info.mDirectory}\\";
                        info.mDirectory = info.mDirectory.Replace('/', '\\');
                        info.mDirectory = info.mDirectory.Replace("\\\\", "\\");
                        info.mFileName = file.Name;
                        mFiles.Add(info);
                    }
                }
            }
        }
    }
}
