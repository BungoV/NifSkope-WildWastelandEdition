# Lane SHOWMENU1: the Cell panel's switches as Windows UI Automation sees them, for an exe with no
# WW_SHOWMENU_GATE driver (the red on main's exe). Writes cell_showmenu.py's dump format:
#   BOX <tab> QCheckBox|QComboBox <tab> (automation id) <tab> label     -- every checkbox / combo in the window
#   TICK <tab> <tab> label <tab> <tab> state                            -- the Show menu's items, if it opens
# Reads only; the Show menu is opened with ExpandCollapse when the button offers it, never a click.
param( [int] $ProcessId, [string] $Out )
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$A = [System.Windows.Automation.AutomationElement]
$T = [System.Windows.Automation.ControlType]
$byPid = New-Object System.Windows.Automation.PropertyCondition( $A::ProcessIdProperty, $ProcessId )
$win = $A::RootElement.FindFirst( [System.Windows.Automation.TreeScope]::Children, $byPid )
$lines = New-Object System.Collections.Generic.List[string]
if ( $win -eq $null ) { $lines.Add( "NOWINDOW" ) }
else {
	foreach ( $pair in @( @( $T::CheckBox, 'QCheckBox' ), @( $T::ComboBox, 'QComboBox' ) ) ) {
		$c = New-Object System.Windows.Automation.PropertyCondition( $A::ControlTypeProperty, $pair[0] )
		foreach ( $e in $win.FindAll( [System.Windows.Automation.TreeScope]::Descendants, $c ) ) {
			$lines.Add( "BOX`t$($pair[1])`t$($e.Current.AutomationId)`t$($e.Current.Name)" )
		}
	}
	$bc = New-Object System.Windows.Automation.AndCondition(
		( New-Object System.Windows.Automation.PropertyCondition( $A::ControlTypeProperty, $T::Button ) ),
		( New-Object System.Windows.Automation.PropertyCondition( $A::NameProperty, 'Show' ) ) )
	$btn = $win.FindFirst( [System.Windows.Automation.TreeScope]::Descendants, $bc )
	if ( $btn -eq $null ) { $lines.Add( "NOSHOWBUTTON" ) }
	else {
		$ec = $null
		if ( $btn.TryGetCurrentPattern( [System.Windows.Automation.ExpandCollapsePattern]::Pattern, [ref] $ec ) ) {
			$ec.Expand()
			Start-Sleep -Milliseconds 800
			$mc = New-Object System.Windows.Automation.PropertyCondition( $A::ControlTypeProperty, $T::MenuItem )
			# the popup is a top-level Menu window of this process; the menu bar's items are not under one
			$pc = New-Object System.Windows.Automation.AndCondition( $byPid,
				( New-Object System.Windows.Automation.PropertyCondition( $A::ControlTypeProperty, $T::Menu ) ) )
			foreach ( $pop in $A::RootElement.FindAll( [System.Windows.Automation.TreeScope]::Children, $pc ) ) {
				foreach ( $m in $pop.FindAll( [System.Windows.Automation.TreeScope]::Children, $mc ) ) {
					$lines.Add( "TICK`t`t$($m.Current.Name)`t`t-" )
				}
			}
			$ec.Collapse()
		} else { $lines.Add( "NOEXPAND" ) }
	}
}
[System.IO.File]::WriteAllLines( $Out, $lines )
"uia: $($lines.Count) lines"
