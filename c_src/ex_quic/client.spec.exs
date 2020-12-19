module ExQuic.Client

interface CNode

state_type "CNodeState"

spec init(local_ip :: string, local_port :: int, remote_ip :: string, remote_port :: int) :: {:ok :: label, state}

