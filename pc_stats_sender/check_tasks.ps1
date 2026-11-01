$tasks = Get-ScheduledTask | Where-Object { $_.State -eq 'Running' -or $_.State -eq 'Ready' }
foreach ($task in $tasks) {
    $actions = $task.Actions
    foreach ($action in $actions) {
        if ($action.Execute -match 'vbs|python|pc_stats') {
            Write-Host "Task: $($task.TaskName)"
            Write-Host "  Execute: $($action.Execute)"
            Write-Host "  Arguments: $($action.Arguments)"
            Write-Host ""
        }
    }
}
