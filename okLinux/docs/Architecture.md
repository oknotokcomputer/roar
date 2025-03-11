{{ #include _header.md }}

# Architecture

{{ #include _wip.md }}

okLinux is a rootless Linux distribution. okLinux is meant to be run
inside a virtual machine. The virtual machine is managed by a fork of
crosvm called `bldy`. 

Bldy uses [`JetStream`](https://github.com/sevki/jetstream) for efficient transport of 9p over Quic for remote connections. As shown in the diagram below, the host machine runs the guest VM, which runs the bldy VM. The bldy VM mounts a remote file server (pangea) over 9p.

     ┌────────────────────────────────────────────────┐                      
     │  remote file server (pangea)                   │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     │                                                │                      
     └────────────────────────────────────────────────┘                      
              ▲                                                              
              │                                                              
              │                                                              
              │              ┌──────────────────────────────────────────────┐
              │              │                                              │
              │              │  host machine         ┌────────────────────┐ │
              │              │                       │ guest VM           │ │
              │              │                       │                    │ │
              │              │                       │                    │ │
              │              │                       │                    │ │
              │              │                       │                    │ │
              │              │  ┌─────────────────┐  │                    │ │
              │              │  │ bldy            │  │                    │ │
              │              │  │                 │  │                    │ │
              │              │  │                 │  │                    │ │
              │              │  │                 │  │                    │ │
              │              │  │                 │  │                    │ │
              │              │  │                 │  │                    │ │
              └──────────────┼──┼────────┐        │  │                    │ │
              JetStream      │  │        │        │  │                    │ │
              9p over Quic   │  │        │        │  │                    │ │
                             │  │        │        │  │                    │ │
                             │  │        │        │  │                    │ │
                             │  │        │        │  │                    │ │
                             │  │        │        │  │                    │ │
                             │  │ 9p over│virtio  │  │                    │ │
                             │  │        └◄───────┼──┤                    │ │
                             │  │                 │  │                    │ │
                             │  │                 │  │                    │ │
                             │  └─────────────────┘  └────────────────────┘ │
                             │                                              │
                             └──────────────────────────────────────────────┘
