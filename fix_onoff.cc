// ... (após a seção de rotas estáticas nos STAs)

// REMOVER OU COMENTAR ESTE BLOCO DE CÓDIGO (Apps de teste originais):
// UdpEchoServerHelper echoServer (9);
// ApplicationContainer serverApps = echoServer.Install (wifiStaNodes3.Get (0));
// ...
// UdpEchoClientHelper echoClient (wifiInterfaces3.GetAddress (0, 1), 9);
// ...
// ApplicationContainer clientApps1 = echoClient.Install (wifiStaNodes2.Get (2));
// ...
   
// --------------------------------------------------------------------------------
// NOVO BLOCO: TRÁFEGO SEQUENCIAL CONTROLADO (Rede 2 -> AP2)
// --------------------------------------------------------------------------------

    // 1. Configuração do Receptor (Sink) no AP2 (n1)
    Ptr<Node> ap2_receptor = wifiApNode2.Get(0); // AP2 (n1)
    uint16_t sinkPort = 9002;
    
    PacketSinkHelper sinkHelper(
      "ns3::UdpSocketFactory",
      Inet6SocketAddress(Ipv6Address::GetAny(), sinkPort)
    );
    ApplicationContainer sinkApp = sinkHelper.Install(ap2_receptor);
    sinkApp.Start(Seconds(0.5)); // Começa cedo
    sinkApp.Stop(Seconds(25.0)); // Para cedo

    // 2. Configuração do Emissor (OnOff)
    
    // O AP2 está na rede 2001:4::/64. O AP2 é o sink.
    Ipv6Address ap2_address = apInterfaces2.GetAddress(0, 1); 

    OnOffHelper onoff("ns3::UdpSocketFactory",
        Address(Inet6SocketAddress(ap2_address, sinkPort)));
    
    // Taxa baixa para garantir que o AP possa receber (ex: 100kbps)
    onoff.SetAttribute("DataRate", StringValue("100kbps"));
    // Envia apenas UM pacote por intervalo, para garantir que o AP consiga processar
    onoff.SetAttribute("PacketSize", UintegerValue(1000)); // Tamanho do pacote em bytes
    // O "OnTime" será o tempo de transmissão de um único pacote (muito curto)
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]")); 
    // O "OffTime" deve ser um tempo grande para o nó não repetir o envio
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=1000]"));

    // 3. Agendamento Sequencial
    double start_offset = 1.0; // Tempo inicial de start
    double interval = 0.05;     // Intervalo entre o start de cada nó (50ms)
    
    // Apenas nos nós da Rede 2 (wifiStaNodes2)
    for (uint32_t i = 0; i < wifiStaNodes2.GetN(); i++)
    {
      // Cria uma instância do OnOffHelper para cada nó
      ApplicationContainer clientApp = onoff.Install(wifiStaNodes2.Get(i));
      
      // Agenda o início da transmissão do nó 'i'
      clientApp.Start(Seconds(start_offset + i * interval));
      clientApp.Stop(Seconds(start_offset + i * interval + 1.0)); // Roda por 1 segundo apenas
    }


    Simulator::Stop(Seconds(30.0)); // Reduz o tempo total da simulação

    // ... (restante do código)
