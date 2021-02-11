module ExQuic.Client

interface CNode

state_type "State"

spec init(remote_ip :: string, remote_port :: int) :: {:ok :: label, state}

spec send_payload(state, data :: payload) :: {:ok :: label, state}

sends {:quic_payload :: label, payload :: payload}
