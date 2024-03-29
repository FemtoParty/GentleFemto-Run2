#include "DmesonTools.h"

/// =====================================================================================
void fitDminus(TString InputDir, TString trigger,
               int errorVar, int potential) {
  gROOT->ProcessLine("gErrorIgnoreLevel = 3001");
  const int nBoot = 1000;

  TString OutputDir = InputDir;
  OutputDir += "/fit/";

  int nSystVars = -1;
  TString errorName;
  if (errorVar == 0) {
    std::cout << "PROCESSING TOTAL ERRORS\n";
    errorName = "totErr";
    nSystVars = 20;
  } else if (errorVar == 1) {
    std::cout << "PROCESSING STATISTICAL ERRORS ONLY\n";
    errorName = "statErr";
    nSystVars = 0;
  }

  TString potName;
  if (potential == 0) {
    potName = "flat";
  } else if (potential == 1) {
    potName = "Coulomb";
  } else if (potential == 2) {
    potName = "Haidenbauer";
  } else if (potential == 3) {
    potName = "Model1";
  } else if (potential == 4) {
    potName = "Model3";
  } else if (potential == 5) {
    potName = "Model4";
  } else {
    std::cout << "ERROR: Potential not defined \n";
    return;
  }

  DreamPlot::SetStyle();

  TRandom3 rangen(0);

  int nArguments = 12;
  TString varList =
      TString::Format(
          "BootID:systID:ppRadius:primaryContrib:flatContrib:dstarContrib:beautyContrib:sidebandContrib:chi2SidebandLeft:chi2SidebandRight:"
          "chi2Local:ndf:nSigma200").Data();
  auto ntResult = new TNtuple("fitResult", "fitResult", varList.Data());
  auto tupleSideband = new TNtuple("sideband", "sideband", "kstar:cf:BootID");
  auto tupleTotalFit = new TNtuple("totalFit", "totalFit", "kstar:cf:BootID");
  auto tupleCorrected = new TNtuple("correctedCF", "correctedCF",
                                    "kstar:cf:BootID");
  auto tupleSidebandLeft = new TNtuple("sidebandLeft", "sidebandLeft",
                                       "kstar:cf:BootID");
  auto tupleSidebandRight = new TNtuple("sidebandRight", "sidebandRight",
                                        "kstar:cf:BootID");

  float ntBuffer[nArguments];
  bool useBaseline = true;

  TString graphfilename = TString::Format("%s/Fit_pDminus_%s_%s.root",
                                          OutputDir.Data(), errorName.Data(),
                                          potName.Data());
  auto param = new TFile(graphfilename, "RECREATE");
  param->mkdir("bootVars");

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// CATS input
  TString CalibBaseDir = "~/cernbox/SystematicsAndCalib/ppRun2_HM/";
  auto calibFile = TFile::Open(
      TString::Format("%s/dstar.root", CalibBaseDir.Data()));
  auto decayKindematicsDstar = (TH2F*) calibFile->Get("histSmearDmeson");

  auto momResFile = TFile::Open(
      TString::Format("%s/momRes_Dmesons.root", CalibBaseDir.Data()).Data());
  auto histMomentumResolutionpDplus = TransformToMeV(
      (TH2F*) momResFile->Get("pDplus"));
  auto histMomentumResolutionpDminus = TransformToMeV(
      (TH2F*) momResFile->Get("pDminus"));
  // since the resolution is the same for p-D+ and p-D- we add them
  auto momentumResolution = histMomentumResolutionpDplus;
  momentumResolution->Add(histMomentumResolutionpDminus);

  std::vector<TGraphAsymmErrors> grCFvec, grSBLeftvec, grSBRightvec;

  const double normLower = 1.5;
  const double normUpper = 2.;
  const int rebin = 10;
  TString dataGrName = "Graph_from_hCk_Reweighted_0MeV";
  TString InputFileName = InputDir;
  InputFileName += "/AnalysisResults.root";

  for (int i = 0; i <= nSystVars; ++i) {
    grCFvec.push_back(
        GetCorrelationGraph(InputFileName, "HM_CharmFemto_", Form("%i", i),
                            dataGrName, normLower, normUpper, rebin));
    grSBLeftvec.push_back(
        GetCorrelationGraph(InputFileName, "HM_CharmFemto_SBLeft_",
                            Form("%i", i), dataGrName, normLower, normUpper,
                            rebin));
    grSBRightvec.push_back(
        GetCorrelationGraph(InputFileName, "HM_CharmFemto_SBRight_",
                            Form("%i", i), dataGrName, normLower, normUpper,
                            rebin));
  }

  std::vector<double> startParams;

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// Set up the CATS ranges, lambda parameters, etc.

  std::vector<double> sidebandFitRange = { { 1500, 1200, 1800 } };

  int nBins = (potential == 0) ? 176 : 96;
  double kmin = 25;
  double kmax = (potential == 0) ? 905 : 505;
  double binWidth = (kmax - kmin) / double(nBins);

  /// Femtoscopic radius systematic variations
  const double rErr = 0.08;
  const double rDefault = 0.89;
  const double rScale = 0.84;
  std::vector<double> sourceSize = { { rDefault, rScale * rDefault - rErr,
      rDefault + rErr } };

  /// Lambda parameters

  /// Proton
  const double protonPurity = 0.984;
  const double protonPrimary = 0.862;
  const double protonLambda = 0.0974;
  const std::vector<double> protonSecondary = { { protonLambda
      / (1. - protonPrimary), protonLambda / (1. - protonPrimary) * 0.8,
      protonLambda / (1. - protonPrimary) * 1.2 } };

  // obtain D-meson properties from Fabrizios files
  std::vector<double> DmesonPurity, DmesonPurityErr;
  std::vector<double> Bfeeddown, BfeeddownErr;
  std::vector<double> DstarFeeding, DstarFeedingErr;

  TString fileAppendix;
  for (int i = 0; i <= nSystVars; ++i) {
    if (i == 0) {
      fileAppendix = "centralcuts";
    } else if (i > 0 && i < 6) {
      fileAppendix = "loose_1";
    } else if (i > 5 && i < 11) {
      fileAppendix = "loose_2";
    } else if (i > 10 && i < 16) {
      fileAppendix = "tight_1";
    } else if (i > 15 && i < 21) {
      fileAppendix = "tight_2";
    }
    auto file = TFile::Open(
        Form("%s/Fractions_masswindow_2sigma_sphericity_0_1_%s.root",
             InputDir.Data(), fileAppendix.Data()));
    auto histFracStat = (TH1F*) file->Get("hFractions_DmPr");
    auto grFracSyst = (TGraphAsymmErrors*) file->Get("gFractionsSyst_DmPr");
    double purity = histFracStat->GetBinContent(1);
    double purityStatErr = histFracStat->GetBinError(1);
    double puritySystErr = (errorVar == 1) ? 0 : grFracSyst->GetErrorY(0);
    DmesonPurity.push_back(purity);
    DmesonPurityErr.push_back(
        std::sqrt(
            purityStatErr * purityStatErr + puritySystErr * puritySystErr));

    double bfeed = histFracStat->GetBinContent(2);
    double bfeedStatErr = histFracStat->GetBinError(2);
    double bfeedSystErr = (errorVar == 1) ? 0 : grFracSyst->GetErrorY(1);
    Bfeeddown.push_back(bfeed);
    BfeeddownErr.push_back(
        std::sqrt(bfeedStatErr * bfeedStatErr + bfeedSystErr * bfeedSystErr));

    double dstarfeed = histFracStat->GetBinContent(3);
    double dstarfeedStatErr = histFracStat->GetBinError(3);
    double dstarfeedSystErr = (errorVar == 1) ? 0 : grFracSyst->GetErrorY(2);
    DstarFeeding.push_back(dstarfeed);
    DstarFeedingErr.push_back(
        std::sqrt(
            dstarfeedStatErr * dstarfeedStatErr
                + dstarfeedSystErr * dstarfeedSystErr));
  }

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// Set up the model, fitter, etc.

  auto tidyCats = new TidyCats();
  CATS cats, catsDstar;
  DLM_Ck *DLM_Coulomb;
  TGraphErrors *grYukiModel;
  if (potential == 0) {
    DLM_Coulomb = new DLM_Ck(1, 0, nBins, kmin, kmax, Flat_Residual);
    DLM_Coulomb->Update();
  } else if (potential == 1) {
    tidyCats->GetCatsProtonDminus(&cats, nBins, kmin, kmax,
                                  TidyCats::pDCoulombOnly, TidyCats::sGaussian);
    cats.SetAnaSource(0, rDefault);
    cats.KillTheCat();
    DLM_Coulomb = new DLM_Ck(1, 0, cats);
  } else if (potential == 2) {
    tidyCats->GetCatsProtonDminus(&cats, nBins, kmin, kmax,
                                  TidyCats::pDminusHaidenbauer,
                                  TidyCats::sGaussian);
    cats.SetAnaSource(0, rDefault);
    cats.KillTheCat();
    DLM_Coulomb = new DLM_Ck(1, 0, cats);
  } else if (potential == 3 || potential == 4 || potential == 5) {
    grYukiModel = getCkFromYuki(potential);
  }

  tidyCats->GetCatsProtonDstarminus(&catsDstar, nBins, kmin, kmax,
                                    TidyCats::pDCoulombOnly,
                                    TidyCats::sGaussian);
  catsDstar.SetAnaSource(0, rDefault);
  catsDstar.KillTheCat();

  auto DLM_pDstar = new DLM_Ck(1, 0, catsDstar);

  auto grSidebandLeft = new TGraphErrors(nBins);
  auto grSidebandRight = new TGraphErrors(nBins);
  int count = 0;
  for (double i = kmin; i <= kmax;) {
    grSidebandLeft->SetPoint(count, i, 0);
    grSidebandRight->SetPoint(count, i, 0);
    i += binWidth;
    count++;
  }

  /// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  /// Systematic variations

  // first iteration is the default
  for (int iBoot = 0; iBoot < nBoot;) {
    if (iBoot % 10 == 0) {
      std::cout << "\r Processing progress: "
                << double(iBoot) / double(nBoot) * 100.f << "%" << std::flush;
    }

    const double femtoRad =
        (iBoot == 0 || errorVar == 1) ?
            sourceSize.at(0) : getBootstrapFromVec(sourceSize);

    // since purity etc. depends on the cut variation, this needs to be done consistently
    int nSystVar = (iBoot == 0) ? 0 : gRandom->Uniform() * grCFvec.size();
    auto grVarCF = grCFvec.at(nSystVar);
    auto grVarCFSidebandLeft = grSBLeftvec.at(nSystVar);
    auto grVarCFSidebandRight = grSBRightvec.at(nSystVar);

    double dMesonPur = DmesonPurity.at(nSystVar);
    double bFeed = Bfeeddown.at(nSystVar);
    double dStarFeed = DstarFeeding.at(nSystVar);

    // now let's sample the uncertainties
    if (iBoot != 0) {
      dMesonPur += DmesonPurityErr.at(nSystVar)
          * std::round(gRandom->Uniform(-1.4999, 1.4999));
      bFeed += BfeeddownErr.at(nSystVar)
          * std::round(gRandom->Uniform(-1.4999, 1.4999));
      dStarFeed += DstarFeedingErr.at(nSystVar)
          * std::round(gRandom->Uniform(-1.4999, 1.4999));
    }

    const double DmesonPrimary = 1.f - bFeed - dStarFeed;

    const Particle dmeson(dMesonPur, DmesonPrimary, { { dStarFeed, bFeed } });

    const double lambdaFeed =
        (iBoot == 0 || errorVar == 1) ?
            protonSecondary.at(0) : getBootstrapFromVec(protonSecondary);
    const Particle proton(
        protonPurity,
        protonPrimary,
        { { (1. - protonPrimary) * lambdaFeed, (1. - protonPrimary)
            * (1 - lambdaFeed) } });

    const CATSLambdaParam lambdaParam(proton, dmeson);

    const double primaryContrib = lambdaParam.GetLambdaParam(
        CATSLambdaParam::Primary);
    const double pDstarContrib = lambdaParam.GetLambdaParam(
        CATSLambdaParam::Primary, CATSLambdaParam::FeedDown, 0, 0);
    double sidebandContrib = lambdaParam.GetLambdaParam(
        CATSLambdaParam::Primary, CATSLambdaParam::Fake, 0, 0);
    sidebandContrib += lambdaParam.GetLambdaParam(CATSLambdaParam::FeedDown,
                                                  CATSLambdaParam::Fake, 0, 0);
    sidebandContrib += lambdaParam.GetLambdaParam(CATSLambdaParam::FeedDown,
                                                  CATSLambdaParam::Fake, 1, 0);
    sidebandContrib += lambdaParam.GetLambdaParam(CATSLambdaParam::Fake,
                                                  CATSLambdaParam::Fake);
    const double flatContrib = 1.f - primaryContrib - sidebandContrib
        - pDstarContrib;

    if (iBoot == 0) {
      std::cout << "Lambda parameters for p-D-\n";
      std::cout << " Primary  " << primaryContrib * 100. << "\n";
      std::cout << " Sideband " << sidebandContrib * 100. << "\n";
      std::cout << " p-D*     " << pDstarContrib * 100. << "\n";
      std::cout << " Flat     " << flatContrib * 100. << "\n";
    }

    auto grCFBootstrapSidebandLeft =
        (iBoot == 0) ?
            &grSBLeftvec.at(0) : getBootstrapGraph(&grVarCFSidebandLeft);
    auto grCFBootstrapSidebandRight =
        (iBoot == 0) ?
            &grSBRightvec.at(0) : getBootstrapGraph(&grVarCFSidebandRight);

    const double sidebandRange =
        (iBoot == 0) ?
            sidebandFitRange.at(0) : getBootstrapFromVec(sidebandFitRange);

    auto fitSidebandLeft = new TF1("fitSidebandLeft", "pol3", 0, sidebandRange);
    getImprovedStartParamsPol3(grCFBootstrapSidebandLeft, sidebandRange,
                               startParams);
    fitSidebandLeft->SetParameters(&startParams[0]);

    int workedLeft = grCFBootstrapSidebandLeft->Fit(fitSidebandLeft, "RQ");
    const double chiSqSidebandLeft = fitSidebandLeft->GetChisquare()
        / double(fitSidebandLeft->GetNDF());

    (TVirtualFitter::GetFitter())->GetConfidenceIntervals(grSidebandLeft,
                                                          0.683);

    auto fitSidebandRight = new TF1("fitSidebandRight", "pol3", 0,
                                    sidebandRange);
    getImprovedStartParamsPol3(grCFBootstrapSidebandRight, sidebandRange,
                               startParams);
    fitSidebandRight->SetParameters(&startParams[0]);

    int workedRight = grCFBootstrapSidebandRight->Fit(fitSidebandRight, "RQ");
    const double chiSqSidebandRight = fitSidebandRight->GetChisquare()
        / double(fitSidebandRight->GetNDF());

    (TVirtualFitter::GetFitter())->GetConfidenceIntervals(grSidebandRight,
                                                          0.683);

    // stop here when the fit fails!
    if (workedLeft != 0 || workedRight != 0 || chiSqSidebandLeft > 50
        || chiSqSidebandRight > 50) {
      std::cout << "Sideband parametrization failed - repeating\n";
      delete fitSidebandLeft;
      delete fitSidebandRight;
      if (iBoot != 0) {
        delete grCFBootstrapSidebandLeft;
        delete grCFBootstrapSidebandRight;
      }
      continue;
    }

    auto totalSideband = WeightedMean(grSidebandLeft, grSidebandRight, 0.51);  // weight extracted from background integral

    if (potential == 3 || potential == 4 || potential == 5) {
      auto grTempYuki = getBootstrapGraph(grYukiModel);
      DLM_Coulomb = getDLMCk(grTempYuki);
      delete grTempYuki;
    }

    DLM_Coulomb->SetSourcePar(0, femtoRad);
    DLM_Coulomb->Update();
    DLM_pDstar->SetSourcePar(0, femtoRad);
    DLM_pDstar->Update();
    auto DLM_sideband = getDLMCk(totalSideband);

    DLM_CkDecomposition CkDec_Coulomb(
        "pDminusTotal", 3, *DLM_Coulomb,
        (potential == 0) ? nullptr : momentumResolution);  // mom res not necessary for flat
    DLM_CkDecomposition CkDec_pDstar("pDstarminus", 0, *DLM_pDstar, nullptr);
    DLM_CkDecomposition CkDec_sideband("sideband", 0, *DLM_sideband, nullptr);

    CkDec_Coulomb.AddContribution(0, pDstarContrib,
                                  DLM_CkDecomposition::cFeedDown, &CkDec_pDstar,
                                  decayKindematicsDstar);
    CkDec_Coulomb.AddContribution(1, flatContrib, DLM_CkDecomposition::cFake);
    CkDec_Coulomb.AddContribution(2, sidebandContrib,
                                  DLM_CkDecomposition::cFeedDown,
                                  &CkDec_sideband);
    CkDec_Coulomb.Update();

    TGraph *grFitTotal = new TGraph();
    TGraph *grFitTotalFine = new TGraph();
    TGraph *grCFRaw = new TGraph();

    double Chi2 = 0;
    double EffNumBins = 0;

    static double mom, dataY, dataBootstrapY, dataErr, theoryX, theoryY;

    auto grCFBootstrap =
        (iBoot == 0) ? &grCFvec.at(0) : getBootstrapGraph(&grVarCF);

    count = 0;
    for (int i = kmin; i < kmax;) {
      double mom = i;
      grCFRaw->SetPoint(count, mom, DLM_Coulomb->Eval(mom));
      grFitTotalFine->SetPoint(count, mom, CkDec_Coulomb.EvalCk(mom));
      tupleTotalFit->Fill(mom, CkDec_Coulomb.EvalCk(mom), iBoot);
      tupleSideband->Fill(mom, totalSideband->Eval(mom), iBoot);
      tupleSidebandLeft->Fill(mom, fitSidebandLeft->Eval(mom), iBoot);
      tupleSidebandRight->Fill(mom, fitSidebandRight->Eval(mom), iBoot);
      i += binWidth;
      count++;
    }

    for (int iPoint = 0; iPoint <= grVarCF.GetN(); ++iPoint) {
      grCFBootstrap->GetPoint(iPoint, mom, dataBootstrapY);
      grVarCF.GetPoint(iPoint, mom, dataY);
      if (mom > 1000) {
        break;
      }

      tupleCorrected->Fill(
          mom,
          1.f
              + 1.f / primaryContrib
                  * (dataBootstrapY - CkDec_Coulomb.EvalCk(mom)),
          iBoot);

      dataErr = grVarCF.GetErrorY(iPoint);

      theoryY = CkDec_Coulomb.EvalCk(mom);
      grFitTotal->SetPoint(iPoint, mom, theoryY);

      if (mom < 200) {
        Chi2 += (dataY - theoryY) * (dataY - theoryY) / (dataErr * dataErr);
        ++EffNumBins;
      }
    }

    double pval = TMath::Prob(Chi2, round(EffNumBins));
    double nSigma = TMath::Sqrt(2) * TMath::ErfcInverse(pval);

    param->cd();
    param->mkdir(TString::Format("bootVars/Graph_%i", iBoot));
    param->cd(TString::Format("bootVars/Graph_%i", iBoot));
    grCFBootstrap->Write("dataBootstrap");

    grCFBootstrapSidebandLeft->Write("dataBootstrapSidebandLeft");
    fitSidebandLeft->Write("sidebandFitLeft");
    grSidebandLeft->Write("sidebandErrorLeft");

    grCFBootstrapSidebandRight->Write("dataBootstrapSidebandRight");
    fitSidebandRight->Write("sidebandFitRight");
    grSidebandRight->Write("sidebandErrorRight");

    totalSideband->SetName("totalSideband");
    totalSideband->Write("sidebandWeightedMean");
    grFitTotal->SetName("TotalFit");
    grFitTotal->Write("TotalFit");
    grFitTotalFine->SetName("TotalFitFine");
    grFitTotalFine->Write("fitFineGrain");

    grCFRaw->Write("raw");

    param->cd();
    ntBuffer[0] = iBoot;
    ntBuffer[1] = nSystVar;
    ntBuffer[2] = femtoRad;
    ntBuffer[3] = primaryContrib;
    ntBuffer[4] = flatContrib;
    ntBuffer[5] = pDstarContrib;
    ntBuffer[6] = bFeed;
    ntBuffer[7] = sidebandContrib;
    ntBuffer[8] = chiSqSidebandLeft;
    ntBuffer[9] = chiSqSidebandRight;
    ntBuffer[10] = Chi2;
    ntBuffer[11] = (float) EffNumBins;
    ntBuffer[12] = nSigma;
    ntResult->Fill(ntBuffer);

    delete fitSidebandLeft;
    delete fitSidebandRight;

    if (iBoot != 0) {
      delete grCFBootstrapSidebandLeft;
      delete grCFBootstrapSidebandRight;
      delete grCFBootstrap;
    }

    delete DLM_sideband;

    delete totalSideband;
    delete grFitTotal;
    delete grFitTotalFine;

    ++iBoot;
  }

/// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/// Exit through the gift shop...

  param->cd();
  ntResult->Write();
  tupleTotalFit->Write();
  tupleSideband->Write();
  tupleCorrected->Write();

  grCFvec.at(0).Write("dataDefault");
  grSBLeftvec.at(0).Write("sidebandLeftDefault");
  grSBRightvec.at(0).Write("sidebandRightDefault");

  auto list = new TList();
  auto fitFull = EvalBootstrap(tupleTotalFit, list, OutputDir, potName, kmin,
                               kmax, binWidth);
  auto dataCorrected = EvalBootstrap(tupleCorrected, &grCFvec.at(0), nullptr,
                                     OutputDir, potName);
  auto sidebandFull = EvalBootstrap(tupleSideband, nullptr, OutputDir, potName,
                                    kmin, kmax, binWidth);
  auto sidebandLeft = EvalBootstrap(tupleSidebandLeft, nullptr, OutputDir,
                                    potName, kmin, kmax, binWidth);
  auto sidebandRight = EvalBootstrap(tupleSidebandRight, nullptr, OutputDir,
                                     potName, kmin, kmax, binWidth);

  fitFull->Write("fitFull");
  sidebandFull->Write("sidebandFull");
  sidebandLeft->Write("sidebandLeft");
  sidebandRight->Write("sidebandRight");

  dataCorrected->Write("correctedData");

  param->Close();

  delete ntResult;
  delete param;
  delete tidyCats;
  return;
}

/// =====================================================================================
int main(int argc, char *argv[]) {
  TString InputDir = argv[1];
  TString trigger = argv[2];
  int errorVar = atoi(argv[3]);
  int potential = atoi(argv[4]);

  fitDminus(InputDir, trigger, errorVar, potential);
}
