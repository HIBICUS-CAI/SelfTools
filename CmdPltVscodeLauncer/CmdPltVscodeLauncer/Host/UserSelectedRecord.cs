// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading;

namespace CmdPltVscodeLauncer
{
    // Port of Wox.Plugin.UserSelectedRecord (PowerToys Run boosts results by how often they were selected).
    public class UserSelectedRecord
    {
        public class UserSelectedRecordItem
        {
            public int SelectedCount { get; set; }

            public DateTime LastSelected { get; set; }

            public string IconPath { get; set; }

            public string Title { get; set; }

            public string Search { get; set; }

            public int Score { get; set; }

            public string SubTitle { get; set; }

            public string PluginID { get; set; }
        }

        private static readonly Lock _lock = new();

        private static string StorageFile => Path.Combine(StoragePaths.LocalDataDirectory, "UserSelectedRecord.json");

        public Dictionary<string, UserSelectedRecordItem> Records { get; set; } = new Dictionary<string, UserSelectedRecordItem>();

        public void Add(Result result, Query originQuery)
        {
            ArgumentNullException.ThrowIfNull(result);

            lock (_lock)
            {
                var key = result.ToString();
                if (Records.TryGetValue(key, out var value))
                {
                    Records[key].SelectedCount = Records[key].SelectedCount + 1;
                    Records[key].LastSelected = DateTime.UtcNow;
                    Records[key].IconPath = null;
                    Records[key].Title = result.Title;
                    Records[key].SubTitle = result.SubTitle;
                    Records[key].Search = (originQuery.Search.Length > 0) ? originQuery.Search : Records[key].Search;

                    if (Records[key].PluginID == null)
                    {
                        Records[key].PluginID = VSCodeWorkspacesPage.PluginID;
                    }
                }
                else
                {
                    Records.Add(key, new UserSelectedRecordItem
                    {
                        SelectedCount = 1,
                        LastSelected = DateTime.UtcNow,
                        Title = result.Title,
                        SubTitle = result.SubTitle,
                        IconPath = null,
                        PluginID = VSCodeWorkspacesPage.PluginID,
                        Search = originQuery.Search,
                    });
                }

                Save();
            }
        }

        public UserSelectedRecordItem GetSelectedData(Result result)
        {
            ArgumentNullException.ThrowIfNull(result);

            lock (_lock)
            {
                if (result != null && Records.TryGetValue(result.ToString(), out var value))
                {
                    return value;
                }
            }

            return new UserSelectedRecordItem { SelectedCount = 0, LastSelected = DateTime.MinValue };
        }

        public static UserSelectedRecord Load()
        {
            try
            {
                if (File.Exists(StorageFile))
                {
                    var record = JsonSerializer.Deserialize(File.ReadAllText(StorageFile), UserSelectedRecordJsonContext.Default.UserSelectedRecord);
                    if (record?.Records != null)
                    {
                        return record;
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Exception($"Failed to load {StorageFile}", ex, typeof(UserSelectedRecord));
            }

            return new UserSelectedRecord();
        }

        private void Save()
        {
            try
            {
                File.WriteAllText(StorageFile, JsonSerializer.Serialize(this, UserSelectedRecordJsonContext.Default.UserSelectedRecord));
            }
            catch (Exception ex)
            {
                Log.Exception($"Failed to save {StorageFile}", ex, typeof(UserSelectedRecord));
            }
        }
    }

    [JsonSerializable(typeof(UserSelectedRecord))]
    [JsonSourceGenerationOptions(WriteIndented = true)]
    internal sealed partial class UserSelectedRecordJsonContext : JsonSerializerContext
    {
    }
}
