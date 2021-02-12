module ExQuic.Server

interface CNode

state_type "State"

spec init(local_ip :: string, local_port :: int) :: {:ok :: label, state}

spec send_payload(state, data :: payload) :: {:ok :: label, state}

sends {:quic_payload :: label, payload :: payload}
