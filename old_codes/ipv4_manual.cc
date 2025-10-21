  // --- ROTAS ESTÁTICAS MANUAIS ---
  Ipv4StaticRoutingHelper staticRoutingHelper;

  // ====== AP1 ======
  Ptr<Ipv4> ipv4Ap1 = apNode1.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> rAp1 = staticRoutingHelper.GetStaticRouting(ipv4Ap1);

  // AP1 conhece Wi-Fi1 local
  // Para alcançar Wi-Fi2 (10.2.2.0/24) → via AP3 (10.1.2.2)
  rAp1->AddNetworkRouteTo(Ipv4Address("10.2.2.0"), Ipv4Mask("255.255.255.0"), i02.GetAddress(1), 3);
  // Para alcançar Wi-Fi3 (10.2.3.0/24) → via AP2 (10.1.1.2)
  rAp1->AddNetworkRouteTo(Ipv4Address("10.2.3.0"), Ipv4Mask("255.255.255.0"), i01.GetAddress(1), 2);

  // ====== AP2 ====== (p2pNodes.Get(1))
  Ptr<Ipv4> ipv4Ap2 = apNode3.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> rAp2 = staticRoutingHelper.GetStaticRouting(ipv4Ap2);

  // Para Wi-Fi1 → via AP1 (10.1.1.1)
  rAp2->AddNetworkRouteTo(Ipv4Address("10.2.1.0"), Ipv4Mask("255.255.255.0"), i01.GetAddress(0), 2);
  // Para Wi-Fi2 → via AP3 (10.1.3.2)
  rAp2->AddNetworkRouteTo(Ipv4Address("10.2.2.0"), Ipv4Mask("255.255.255.0"), i12.GetAddress(1), 3);

  // ====== AP3 ====== (p2pNodes.Get(2))
  Ptr<Ipv4> ipv4Ap3 = apNode2.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> rAp3 = staticRoutingHelper.GetStaticRouting(ipv4Ap3);

  // Para Wi-Fi1 → via AP1 (10.1.2.1)
  rAp3->AddNetworkRouteTo(Ipv4Address("10.2.1.0"), Ipv4Mask("255.255.255.0"), i02.GetAddress(0), 2);
  // Para Wi-Fi3 → via AP2 (10.1.3.1)
  rAp3->AddNetworkRouteTo(Ipv4Address("10.2.3.0"), Ipv4Mask("255.255.255.0"), i12.GetAddress(0), 3);

  // ====== GATEWAYS DAS ESTAÇÕES ======
  Ipv4Address gatewayAp1 = ifAp1.GetAddress(0); // 10.2.1.1
  Ipv4Address gatewayAp2 = ifAp3.GetAddress(0); // 10.2.3.1
  Ipv4Address gatewayAp3 = ifAp2.GetAddress(0); // 10.2.2.1

  for (uint32_t i = 0; i < staNet1.GetN(); ++i)
    {
      Ptr<Ipv4> ipv4 = staNet1.Get(i)->GetObject<Ipv4>();
      Ptr<Ipv4StaticRouting> s = staticRoutingHelper.GetStaticRouting(ipv4);
      s->SetDefaultRoute(gatewayAp1, 1);
    }

  for (uint32_t i = 0; i < staNet2.GetN(); ++i)
    {
      Ptr<Ipv4> ipv4 = staNet2.Get(i)->GetObject<Ipv4>();
      Ptr<Ipv4StaticRouting> s = staticRoutingHelper.GetStaticRouting(ipv4);
      s->SetDefaultRoute(gatewayAp3, 1);
    }

  for (uint32_t i = 0; i < staNet3.GetN(); ++i)
    {
      Ptr<Ipv4> ipv4 = staNet3.Get(i)->GetObject<Ipv4>();
      Ptr<Ipv4StaticRouting> s = staticRoutingHelper.GetStaticRouting(ipv4);
      s->SetDefaultRoute(gatewayAp2, 1);
    }

  // --- DEBUG opcional ---
  // Visualizar tabelas de roteamento
  Ipv4GlobalRoutingHelper g;
  g.PrintRoutingTableAllAt(Seconds(1.5), std::cout);

