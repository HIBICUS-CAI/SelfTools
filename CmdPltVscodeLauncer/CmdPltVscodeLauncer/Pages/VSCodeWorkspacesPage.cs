// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;

using CmdPltVscodeLauncer.Properties;
using CmdPltVscodeLauncer.RemoteMachinesHelper;
using CmdPltVscodeLauncer.VSCodeHelper;
using CmdPltVscodeLauncer.WorkspacesHelper;

using Microsoft.CommandPalette.Extensions;
using Microsoft.CommandPalette.Extensions.Toolkit;

using Windows.System;

namespace CmdPltVscodeLauncer
{
    // Port of Community.PowerToys.Run.Plugin.VSCodeWorkspaces.Main
    internal sealed partial class VSCodeWorkspacesPage : DynamicListPage
    {
        // PowerToys Run sorts by Score + SelectedCount * 5 (SearchQueryTuningEnabled = false, the default)
        private const int SelectedItemMultiplier = 5;

        // plugin.json "Name" (used by PowerToys Run for the error message title)
        private const string PluginMetadataName = "VS Code Workspaces";

        public static string PluginID => "525995402BEF4A8CA860D92F6D108092";

        private readonly VSCodeWorkspacesApi _workspacesApi = new VSCodeWorkspacesApi();

        private readonly VSCodeRemoteMachinesApi _machinesApi = new VSCodeRemoteMachinesApi();

        private readonly UserSelectedRecord _userSelectedRecord = UserSelectedRecord.Load();

        public VSCodeWorkspacesPage()
        {
            Icon = Icons.PluginIcon;
            Title = Resources.PluginTitle;
            Name = "Open";
            Id = "CmdPltVscodeLauncer.VSCodeWorkspaces";
        }

        public override void UpdateSearchText(string oldSearch, string newSearch)
        {
            RaiseItemsChanged(0);
        }

        public override IListItem[] GetItems()
        {
            var query = new Query(SearchText);
            var results = Query(query);

            // PowerLauncher MainViewModel.UpdateResultView + ResultsViewModel.Sort
            foreach (var result in results)
            {
                result.SelectedCount = _userSelectedRecord.GetSelectedData(result).SelectedCount;
            }

            results = results.OrderByDescending(x => x.GetSortOrderScore(SelectedItemMultiplier)).ToList();

            return results.Select(r => CreateListItem(r, query)).ToArray();
        }

        private ListItem CreateListItem(Result result, Query query)
        {
            var command = new ResultCommand(() =>
            {
                var hide = result.Action();
                _userSelectedRecord.Add(result, query);
                return hide;
            });

            return new ListItem(command)
            {
                Title = result.Title,
                Subtitle = result.SubTitle,
                Icon = result.Icon(),
                MoreCommands = LoadContextMenus(result)
                    .Select(c => (IContextItem)new CommandContextItem(new ResultCommand(c.Action))
                    {
                        Title = c.Title,
                        Icon = new IconInfo(c.Glyph),
                        RequestedShortcut = c.RequestedShortcut,
                    })
                    .ToArray(),
            };
        }

        public List<Result> Query(Query query)
        {
            var results = new List<Result>();

            if (query != null)
            {
                // Search opened workspaces
                _workspacesApi.Workspaces.ForEach(a =>
                {
                    var title = a.WorkspaceType == WorkspaceType.ProjectFolder ? a.FolderName : a.FolderName.Replace(".code-workspace", $" ({Resources.Workspace})");

                    var typeWorkspace = a.WorkspaceEnvironmentToString();
                    if (a.WorkspaceEnvironment != WorkspaceEnvironment.Local)
                    {
                        title = $"{title}{(a.ExtraInfo != null ? $" - {a.ExtraInfo}" : string.Empty)} ({typeWorkspace})";
                    }

                    results.Add(new Result
                    {
                        Title = title,
                        SubTitle = $"{(a.WorkspaceType == WorkspaceType.WorkspaceFile ? Resources.Workspace : Resources.ProjectFolder)}{(a.WorkspaceEnvironment != WorkspaceEnvironment.Local ? $" {Resources.In} {typeWorkspace}" : string.Empty)}: {SystemPath.RealPath(a.RelativePath)}",
                        Icon = a.VSCodeInstance.WorkspaceIcon,
                        Action = () =>
                        {
                            bool hide;
                            try
                            {
                                var process = new ProcessStartInfo
                                {
                                    FileName = a.VSCodeInstance.ExecutablePath,
                                    UseShellExecute = true,
                                    Arguments = a.WorkspaceType == WorkspaceType.ProjectFolder ? $"--folder-uri {a.Path}" : $"--file-uri {a.Path}",
                                    WindowStyle = ProcessWindowStyle.Hidden,
                                };
                                Process.Start(process);

                                hide = true;
                            }
                            catch (Win32Exception ex)
                            {
                                HandleError("Can't Open this file", ex, showMsg: true);
                                hide = false;
                            }

                            return hide;
                        },
                        ContextData = a,
                    });
                });

                // Search opened remote machines
                _machinesApi.Machines.ForEach(a =>
                {
                    var title = $"{a.Host}";

                    if (a.User != null && a.User != string.Empty && a.HostName != null && a.HostName != string.Empty)
                    {
                        title += $" [{a.User}@{a.HostName}]";
                    }

                    results.Add(new Result
                    {
                        Title = title,
                        SubTitle = Resources.SSHRemoteMachine,
                        Icon = a.VSCodeInstance.RemoteIcon,
                        Action = () =>
                        {
                            bool hide;
                            try
                            {
                                var process = new ProcessStartInfo()
                                {
                                    FileName = a.VSCodeInstance.ExecutablePath,
                                    UseShellExecute = true,
                                    Arguments = $"--new-window --enable-proposed-api ms-vscode-remote.remote-ssh --remote ssh-remote+{((char)34) + a.Host + ((char)34)}",
                                    WindowStyle = ProcessWindowStyle.Hidden,
                                };
                                Process.Start(process);

                                hide = true;
                            }
                            catch (Win32Exception ex)
                            {
                                HandleError("Can't Open this file", ex, showMsg: true);
                                hide = false;
                            }

                            return hide;
                        },
                        ContextData = a,
                    });
                });
            }

            results = results.Where(a => a.Title.Contains(query.Search, StringComparison.InvariantCultureIgnoreCase)).ToList();

            results.ForEach(x =>
            {
                if (x.Score == 0)
                {
                    x.Score = 100;
                }

                // intersect the title with the query
                var intersection = Convert.ToInt32(x.Title.ToLowerInvariant().Intersect(query.Search.ToLowerInvariant()).Count() * query.Search.Length);
                var differenceWithQuery = Convert.ToInt32((x.Title.Length - intersection) * query.Search.Length * 0.7);
                x.Score = x.Score - differenceWithQuery + intersection;

                // if is a remote machine give it 12 extra points
                if (x.ContextData is VSCodeRemoteMachine)
                {
                    x.Score = Convert.ToInt32(x.Score + (intersection * 2));
                }
            });

            results = results.OrderByDescending(x => x.Score).ToList();
            if (query.Search == string.Empty || query.Search.Replace(" ", string.Empty) == string.Empty)
            {
                results = results.OrderBy(x => x.Title).ToList();
            }

            return results;
        }

        public static List<ContextMenuResult> LoadContextMenus(Result selectedResult)
        {
            if (selectedResult?.ContextData is not VSCodeWorkspace workspace)
            {
                return new List<ContextMenuResult>();
            }

            string realPath = SystemPath.RealPath(workspace.RelativePath);

            return new List<ContextMenuResult>
            {
                new ContextMenuResult
                {
                    Title = $"{Resources.CopyPath} (Ctrl+C)",
                    Glyph = "\xE8C8", // Copy
                    RequestedShortcut = KeyChordHelpers.FromModifiers(ctrl: true, vkey: VirtualKey.C),
                    Action = () => CopyToClipboard(realPath),
                },
                new ContextMenuResult
                {
                    Title = $"{Resources.OpenInExplorer} (Ctrl+Shift+F)",
                    Glyph = "\xEC50", // File Explorer
                    RequestedShortcut = KeyChordHelpers.FromModifiers(ctrl: true, shift: true, vkey: VirtualKey.F),
                    Action = () => OpenInExplorer(realPath),
                },
                new ContextMenuResult
                {
                    Title = $"{Resources.OpenInConsole} (Ctrl+Shift+C)",
                    Glyph = "\xE756", // Command Prompt
                    RequestedShortcut = KeyChordHelpers.FromModifiers(ctrl: true, shift: true, vkey: VirtualKey.C),
                    Action = () => OpenInConsole(realPath),
                },
            };
        }

        private static bool CopyToClipboard(string path)
        {
            try
            {
                ClipboardHelper.SetText(path);
                return true;
            }
            catch (Exception ex)
            {
                HandleError("Can't copy to clipboard", ex, showMsg: true);
                return false;
            }
        }

        private static bool OpenInConsole(string path)
        {
            try
            {
                Helper.OpenInConsole(path);
                return true;
            }
            catch (Exception ex)
            {
                HandleError($"Unable to open the specified path in the console: {path}", ex, showMsg: true);
                return false;
            }
        }

        private static bool OpenInExplorer(string path)
        {
            if (!Helper.OpenInShell("explorer.exe", $"\"{path}\""))
            {
                HandleError($"Failed to open folder in Explorer at path: {path}", showMsg: true);
                return false;
            }

            return true;
        }

        private static void HandleError(string msg, Exception exception = null, bool showMsg = false)
        {
            if (exception != null)
            {
                Log.Exception(msg, exception, exception.GetType());
            }
            else
            {
                Log.Error(msg, typeof(VSCodeWorkspacesPage));
            }

            if (showMsg)
            {
                new ToastStatusMessage(new StatusMessage
                {
                    Message = $"Plugin: {PluginMetadataName}{Environment.NewLine}{msg}",
                    State = MessageState.Error,
                }).Show();
            }
        }

        public static string GetTranslatedPluginTitle()
        {
            return Resources.PluginTitle;
        }

        public static string GetTranslatedPluginDescription()
        {
            return Resources.PluginDescription;
        }
    }
}
