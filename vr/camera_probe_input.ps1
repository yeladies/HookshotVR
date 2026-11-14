# SPDX-License-Identifier: GPL-3.0-or-later
function Get-VrProbeCameraTurnScript {
    param([ValidateSet('Left','Right')][string]$Direction,[ValidateRange(75,2000)][int]$Milliseconds=150)
    $button=if($Direction -eq 'Left'){'RLEFT'}else{'RRIGHT'}
    return ("0:{0}/{1}`n" -f $button,$Milliseconds)
}
