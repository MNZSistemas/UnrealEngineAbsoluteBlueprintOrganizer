# Unreal Engine Absolute Blueprint Organizer 1.0 - Specification

Plugin `UnrealEngineAbsoluteBlueprintOrganizer`, modules `UeaboCore` (organizer engine + public API) and `UeaboEditor`
(commands, menus, settings, report tab, automation tests). Editor-only, Win64. Engines: UE 4.27, 5.0,
5.5, 5.6, 5.7, 5.8. Publisher MNZ Sistemas. Header on every source file:
`// Copyright (c) 2026 MNZ Sistemas. All rights reserved.`

## Coding rules
C++17 (do not set CppStandard), no TObjectPtr in our code, compile-time engine branches only
(`UEABO_ENGINE_AT_LEAST(Major, Minor)` in `UeaboCore/Public/UeaboVersion.h`), all version shims in
`UeaboCore/Private/UeaboEngineCompat.h/.cpp`. No fallbacks: one path per engine version; when something
cannot be done, return an explicit error in `FUeaboOrganizeResult::Error`. Never let user data reach a
`check()`. No mention of AI vendors anywhere.

## Commands (UeaboEditor)
| Command | Default chord | Action |
|---|---|---|
| `OrganizeGraph` | Ctrl+Alt+L | organize the Blueprint editor's focused graph |
| `OrganizeBlueprint` | Ctrl+Alt+Shift+L | organize every graph + members |
| `OpenReport` | none | open the report tab |

`TCommands<FUeaboCommands>` (context name `UnrealEngineAbsoluteBlueprintOrganizer`, parent `EditorViewport`-free:
use `NAME_None` parent); default chords come from `UUeaboSettings` at registration (a chord change needs
an editor restart). Mapped into the Blueprint editor's own command list via
`FBlueprintEditorModule::GetMenuExtensibilityManager()->GetExtenderDelegates()` (the delegate receives
the editor's `FUICommandList`, which is where we `MapAction`), plus a toolbar extender (combo button
"Organize" with the three entries) through the same module's toolbar extensibility manager. The
executed action finds the `FBlueprintEditor` via `UAssetEditorSubsystem::FindEditorForAsset` (4.27+)
and uses `GetFocusedGraph()`.

## Settings `UUeaboSettings : UDeveloperSettings`
`Config=EditorPerProjectUserSettings`, category `Plugins`, section `Unreal Engine Absolute Blueprint Organizer`.
Shortcuts: `OrganizeGraphChord`, `OrganizeBlueprintChord` (FInputChord).
Layout: `HorizontalSpacing`=80, `VerticalSpacing`=40, `bInsertReroutes`=true, `bStraightenLinks`=true.
Comments: `bCreateComments`=true, `ClusterCommentThreshold`=8, `CommentPadding`=30,
`EventColor`, `InputColor`, `TimerColor`, `UIColor`, `NetworkColor`, `DefaultColor` (FLinearColor).
Hygiene: `bRemoveOrphanNodes`, `bRemoveNoOpReroutes`, `bMergeDuplicateCasts` (all true).
Members: `bSortVariables`, `bAutoCategorize`, `CategoryRules` (TArray<FUeaboCategoryRule>{FString
Pattern; EUeaboMatch Match {Prefix,Suffix,Contains}; FString Category}), `bEnforceBoolPrefix`,
`bEnforcePascalCase`, `bSortFunctions` (all true). Report: `bShowReportAfterOrganize`=true,
`LongChainThreshold`=15, `BranchDepthThreshold`=3.
`FUeaboOrganizeOptions` (UeaboCore, plain USTRUCT mirroring these values) is what the core consumes;
`UeaboEditor` builds it from settings. The core never reads settings (so the API is usable headless).

## Public API (UeaboCore/Public/IUeaboOrganizer.h)
```cpp
class UEABOCORE_API IUeaboOrganizer : public IModuleInterface {
public:
    static IUeaboOrganizer& Get();            // loads "UeaboCore"
    virtual FUeaboOrganizeResult OrganizeGraph(UEdGraph* Graph, const FUeaboOrganizeOptions& Options) = 0;
    virtual FUeaboOrganizeResult OrganizeBlueprint(UBlueprint* Blueprint, const FUeaboOrganizeOptions& Options) = 0;
};
```
`FUeaboOrganizeResult`: `bSuccess`, `Error`, `Before`/`After` (`FUeaboGraphStats`: Nodes, Crossings,
Comments, Orphans), `OrphansRemoved`, `ReroutesRemoved`, `ReroutesInserted`, `CastsMerged`,
`CommentsCreated`, `CommentsResized`, `VariablesRenamed`, `VariablesRecategorized`,
`TArray<FString> Suggestions`, `TArray<FString> Notes` (hygiene findings reported, not changed).
Each call opens exactly one `FScopedTransaction` ("Organize Blueprint"), `Modify()` on every touched
object; OrganizeBlueprint does not nest a transaction per graph. Ends with
`FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified` when nodes/members changed, else
`MarkBlueprintAsModified`.

## Algorithms (UeaboCore)
Pipeline per graph: hygiene -> layout -> reroutes -> comments -> stats. Members after all graphs.

1. Node size: rendered size if the graph is open (SGraphNode via the cheat-sheet route, desired size),
   else stored `NodeWidth/NodeHeight` when > 0, else estimate: width = 16 + 8*max(title chars, longest
   pin label on each side combined) clamped [120, 480]; height = 32 + 24*max(input rows, output rows).
   One path per case, decided by what is present (a documented input form, not a fallback).
2. Roots: event nodes (UK2Node_Event incl. custom/input), function/macro entry (UK2Node_FunctionEntry,
   UK2Node_Tunnel entry). Exec rank = longest exec-path distance from a root (cycles broken by DFS
   back-edge removal). Exec nodes unreachable from any root get rank from their own island.
3. Ordering within rank: barycenter heuristic, 4 down/up sweeps, keep best crossing count.
4. Placement: each root starts a lane (a band); lanes stack vertically with `VerticalSpacing*3` gap.
   X of rank r = max over previous ranks' widths + HorizontalSpacing. Y aligned to the exec input pin
   of the node's primary exec predecessor (straighten), resolving overlaps by pushing down.
5. Data nodes (no exec pins): placed in columns left of their first consumer, Y aligned to the consumer
   input pin they feed; shared data nodes go by their leftmost consumer; stacked with VerticalSpacing.
6. Reroutes: after placement, for each link whose straight segment intersects a third node's bounds,
   insert one UK2Node_Knot above that node (only when `bInsertReroutes`).
7. Comments: one per lane, titled from the root (event name / function name), coloured by category
   (Input: InputAction/InputKey/InputAxis/EnhancedInput events; Timer: names containing Timer/Delay/Tick;
   UI: Widget/UMG/HUD/Construct; Network: Server/Client/Multicast/RPC/Replicat/OnRep; Event: other).
   Existing comment matching by title (NodeComment equals our title) or by containing the root: resized
   to the lane bounds + padding, never duplicated. Function-call clusters (a run of > threshold
   consecutive exec nodes calling functions of the same target class) get a nested comment titled by
   that class.
8. Hygiene: orphan = node with no linked pins, not an event/entry/result/comment/knot-with-links;
   no-op reroute = knot with exactly one input link and outputs, bypassed then removed (or knot
   with no links); duplicate casts = two UK2Node_DynamicCast with the same TargetType and the same
   source output pin within the same exec chain where the later one is dominated by the earlier:
   rewire the later's outputs to the earlier's result pin and splice its exec. Anything else
   (unused variables, unconnected pins, pure-candidate functions) goes to Notes.
9. Members: sort `NewVariables` by (Category, Name); auto-category by first matching rule when the
   category is empty/Default; bools without `b` prefix -> `b` + PascalCase; non-PascalCase names ->
   PascalCase (strip `_`, capitalise words); rename with `FBlueprintEditorUtils::RenameMemberVariable`
   (skip with a Note when the target name already exists). Sort FunctionGraphs, MacroGraphs,
   DelegateSignatureGraphs by name. Never mark Pure; add a suggestion instead.
10. Suggestions: exec chains longer than LongChainThreshold -> "extract function" candidate;
    Branch nesting deeper than BranchDepthThreshold.

## Report tab (UeaboEditor)
Nomad tab `UeaboReport` under Window menu, `SListView` of the last result: before/after table and
suggestions; opened after organize when `bShowReportAfterOrganize`.

## Tests (UeaboEditor/Private/Tests)
`Ueabo.Organizer.MessyBlueprint`: builds a transient Blueprint (Actor parent) with ~40 nodes (3 events,
exec chains of PrintString/Delay/Branch/Sequence, pure math nodes, 4 orphans, 2 duplicate casts on one
chain, 2 no-op reroutes, one existing comment), random positions; runs OrganizeBlueprint; asserts:
no overlapping node bounds (non-comment), every exec link target X > source X, orphans removed, a
comment per event, duplicates merged, then `GEditor->UndoTransaction()` restores the recorded
positions. `Ueabo.Organizer.Members`: renames/sorting rules. Run with `Tools/run_tests.ps1`.

## API cheat sheet (verified on 4.27.2-release and 5.8.2-release)
See the section appended below.

### Verified findings (4.27.2 / 5.8.2)
| Topic | Finding / rule |
|---|---|
| Toolbar | No Blueprint-editor toolbar extensibility manager on either tag. Toolbar: `UToolMenus` menu `"AssetEditor.BlueprintEditor.ToolBar"`-family; `"Kismet.Toolbar"`-style names must be checked with `UToolMenus::Get()->FindMenu` naming on each tag (UToolMenus exists on both). Commands: `FBlueprintEditorModule::GetMenuExtensibilityManager()->GetExtenderDelegates().Add(FAssetEditorExtender)` (delegate gets the editor's `TSharedRef<FUICommandList>`, map actions there); `OnGatherBlueprintMenuExtensions` also available. |
| Node size | `UEdGraphNode::NodeWidth/NodeHeight` exist on both (used by comments / resizable nodes). Rendered size: `SGraphPanel::GetNodeWidgetFromGuid(NodeGuid)` -> `SGraphNode::GetDesiredSize()`; panel from the open editor's `SGraphEditor::GetGraphPanel()`. |
| BlueprintEditorUtils | `RenameMemberVariable`, `SetBlueprintVariableCategory`, `RemoveNode`, `MarkBlueprintAs(Structurally)Modified`, `FKismetEditorUtilities::CreateBlueprint` unchanged 4.27 -> 5.8. |
| DynamicCast | `bIsPureCast` deprecated 5.3: use `IsNodePure()` / `SetPurity()` on all tags. |
| Automation flags | namespace (4.27) -> struct (5.3) -> enum class (5.5+); `EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter` compiles on all. |
| Tabs | `FGlobalTabmanager::TryInvokeTab(FTabId)` gained an optional bool; call with one arg. |
| UBlueprint arrays | `TObjectPtr` on 5.x; implicit conversion to raw pointers OK, don't spell TObjectPtr. |
| Style | `FEditorStyle` (4.27, module EditorStyle) vs `FAppStyle` (5.0+, Slate) - compile-time branch in one shim. |
| DeveloperSettings | module `DeveloperSettings` on both tags. |
Anything not listed: verify with `git show <tag>:<path>` on 4.27.2-release and 5.8.2-release before use.

## Implementation notes 1.0 (deviations from the text above)
- Rendered node size: `SGraphEditor::FindGraphEditorForGraph(Graph)->GetBoundsForNode()` (public on all tags; `GetGraphPanel` is absent on 4.27). Reroute knots use a fixed 42x16 when not rendered. Public helper `UeaboNodeMetrics::GetNodeSize/GetTitleLine`.
- Roots = any exec node without an exec input pin (covers events, entries, input events).
- Data nodes that feed no exec node are laid out in a final "data lane" without a comment.
- Case-only variable renames (e.g. `zeta` -> `Zeta`) are reported as a Note: FName is case-insensitive, `RenameMemberVariable` treats them as no-ops.
- `UeaboEditor` loads at `PostEngineInit` (Kismet asserts on GEditor at Default).
- Commands use style set `EditorStyle` (TCommands asserts on a None style set).
- UE 5.0 only (Build.cs `#if !UE_5_1_OR_LATER`): C4668 off and `__has_feature(x)=0` for engine headers on current MSVC.
- UE 4.27 UHT hangs at "Loading text-based GConfig" when Config/FilterPlugin.ini contains a `;` comment line: keep that file comment-free.
