  // --- ROTAS MANUAIS ---

  Ipv4StaticRoutingHelper staticRoutingHelper;

  // ===== AP1 =====
  Ptr<Ipv4> ipv4Ap1 = apNode1.Get(0)->GetObject<Ipv4>();
  Ptr<Ipv4StaticRouting> staticAp1 = staticRoutingHelper.GetStaticRouting(ipv4Ap1);
  // Interface indices: (você pode confirmar com Ipv4::GetNInterfaces())
  // - iface 1: Wi-Fi1 (10.2.1.1)
  // - iface 2: P2P com AP2 (10.1.1.1)
  // - iface 3: P2P com AP3 (10.1.2.1)

  staticAp1->AddNetworkRouteTo(Ipv4Address("10.2.2.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.2.2"), 3); // via AP3
  staticAp1->AddNetworkRouteTo(Ipv4Address("10.2.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.1.2"), 2); // via AP2

  // ===== AP2 =====
  Ptr<Ipv4> ipv4Ap2 = apNode3.Get(0)->GetObject<Ipv4>(); // apNode3 é AP2 (p2pNodes.Get(1))
  Ptr<Ipv4StaticRouting> staticAp2 = staticRoutingHelper.GetStaticRouting(ipv4Ap2);
  // iface 1: Wi-Fi3 (10.2.3.1)
  // iface 2: P2P com AP1 (10.1.1.2)
  // iface 3: P2P com AP3 (10.1.3.1)

  staticAp2->AddNetworkRouteTo(Ipv4Address("10.2.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.1.1"), 2); // via AP1
  staticAp2->AddNetworkRouteTo(Ipv4Address("10.2.2.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.2"), 3); // via AP3

  // ===== AP3 =====
  Ptr<Ipv4> ipv4Ap3 = apNode2.Get(0)->GetObject<Ipv4>(); // apNode2 é AP3 (p2pNodes.Get(2))
  Ptr<Ipv4StaticRouting> staticAp3 = staticRoutingHelper.GetStaticRouting(ipv4Ap3);
  // iface 1: Wi-Fi2 (10.2.2.1)
  // iface 2: P2P com AP1 (10.1.2.2)
  // iface 3: P2P com AP2 (10.1.3.2)

  staticAp3->AddNetworkRouteTo(Ipv4Address("10.2.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.2.1"), 2); // via AP1
  staticAp3->AddNetworkRouteTo(Ipv4Address("10.2.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.1.3.1"), 3); // via AP2

  // ===== STA DEFAULT ROUTES =====
  Ipv4Address gatewayAp1 = ifAp1.GetAddress(0);
  Ipv4Address gatewayAp2 = ifAp2.GetAddress(0);
  Ipv4Address gatewayAp3 = ifAp3.GetAddress(0);

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

