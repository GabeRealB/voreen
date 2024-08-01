/***********************************************************************************
 *                                                                                 *
 * Voreen - The Volume Rendering Engine                                            *
 *                                                                                 *
 * Copyright (C) 2005-2024 University of Muenster, Germany,                        *
 * Department of Computer Science.                                                 *
 * For a list of authors please refer to the file "CREDITS.txt".                   *
 *                                                                                 *
 * This file is part of the Voreen software package. Voreen is free software:      *
 * you can redistribute it and/or modify it under the terms of the GNU General     *
 * Public License version 2 as published by the Free Software Foundation.          *
 *                                                                                 *
 * Voreen is distributed in the hope that it will be useful, but WITHOUT ANY       *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR   *
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.      *
 *                                                                                 *
 * You should have received a copy of the GNU General Public License in the file   *
 * "LICENSE.txt" along with this file. If not, see <http://www.gnu.org/licenses/>. *
 *                                                                                 *
 * For non-commercial academic use see the license exception specified in the file *
 * "LICENSE-academic.txt". To get information about commercial licensing please    *
 * contact the authors.                                                            *
 *                                                                                 *
 ***********************************************************************************/

#include "cosmologyyearprovider.h"

#include <iomanip>

const float gyrAtStep[625] = { 13.661603766037631, 13.65798483046405, 13.65399315842606, 13.64966078103741, 13.645012651570305, 13.640068925513967, 13.634846327292067, 13.62935902665077, 13.62361923023214, 13.61763759739339, 13.61142354223383,
13.604985459005213, 13.59833089422849, 13.591466680705505, 13.584399043635003, 13.577133685884782, 13.569675857407779, 13.562030412403958, 13.55420185687743, 13.546194388569957, 13.538011930774145,
13.5296581611825, 13.521136536672135, 13.512450314733298, 13.503602572104636, 13.494596221066798, 13.485434023759884, 13.476118604822735, 13.466652462598999, 13.457037979112506, 13.4472774289806,
13.437372987406635, 13.427326737370565, 13.417140676118292, 13.406816721035392, 13.396356714978447, 13.385762431126768, 13.375035577408669, 13.364177800549182, 13.353190689779868, 13.342075780246223,
13.330834556143705, 13.31946845360959, 13.307978863394615, 13.29636713333556, 13.284634570647453, 13.272782444052044, 13.260811985757273, 13.24872439330095, 13.23652083127039, 13.224202432908577,
13.21177030161632, 13.199225512358927, 13.186569112985076, 13.173802125464846, 13.160925547053145, 13.147940351384289, 13.134847489502837, 13.12164789083545, 13.108342464107997, 13.094932098211872,
13.081417663023068, 13.067800010177262, 13.054079973803956, 13.040258371222384, 13.026336003601719, 13.012313656587917, 12.998192100899345, 12.983972092893142, 12.969654375104163, 12.95523967675819,
12.94072871426095, 12.926122191664431, 12.911420801111772, 12.89662522326204, 12.881736127696016, 12.866754173304052, 12.851680008657054, 12.83651427236146, 12.821257593399153, 12.805910591453047,
12.79047387721918, 12.774948052705975, 12.759333711521355, 12.74363143914833, 12.727841813209642, 12.711965403722006, 12.696002773340487, 12.679954477593464, 12.663821065108657, 12.64760307783064,
12.63130105123023, 12.614915514506151, 12.598446990779317, 12.581895997280068, 12.56526304552869, 12.548548641509505, 12.531753285838823, 12.514877473927028, 12.497921696135027, 12.48088643792535,
12.46377218000806, 12.446579398481772, 12.429308564969896, 12.411960146752365, 12.394534606893016, 12.377032404362764, 12.35945399415878, 12.341799827419814, 12.32407035153779, 12.306266010265851,
12.288387243822982, 12.270434488995301, 12.252408179234205, 12.23430874475143, 12.21613661261118, 12.197892206819391, 12.17957594841027, 12.16118825553018, 12.142729543518984, 12.124200224988897,
12.105600709900996, 12.086931405639392, 12.068192717083207, 12.049385046676395, 12.030508794495475, 12.011564358315269, 11.99255213367268, 11.973472513928604, 11.954325890327997, 11.935112652058194,
11.915833186305504, 11.896487878310145, 11.877077111419585, 11.857601267140296, 11.838060725188026, 11.818455863536563, 11.798787058465107, 11.779054684604239, 11.759259114980537, 11.739400721059912,
11.719479872789634, 11.699496938639157, 11.679452285639721, 11.659346279422797, 11.63917928425738, 11.618951663086204, 11.598663777560834, 11.578315988075762, 11.55790865380143, 11.537442132716306,
11.516916781637947, 11.496332956253154, 11.475691011147166, 11.454991299832002, 11.43423417477389, 11.413419987419848, 11.392549088223458, 11.37162182666978, 11.350638551299529, 11.329599609732412,
11.308505348689767, 11.287356114016433, 11.266152250701897, 11.244894102900743, 11.22358201395242, 11.202216326400318, 11.18079738201021, 11.159325521788013, 11.137801085996971, 11.116224414174184,
11.094595845146545, 11.072915717046104, 11.051184367324836, 11.029402132768867, 11.007569349512128, 10.985686353049491, 10.963753478249366, 10.94177105936579, 10.919739430049999, 10.897658923361528,
10.875529871778804, 10.853352607209285, 10.831127460999127, 10.808854763942396, 10.78653484628984, 10.764168037757234, 10.74175466753328, 10.719295064287119, 10.6967895561754, 10.674238470848993,
10.651642135459264, 10.629000876664012, 10.606315020633001, 10.583584893053136, 10.560810819133286, 10.537993123608741, 10.51513213074535, 10.49222816434328, 10.469281547740502, 10.446292603815909,
10.423261654992134, 10.400189023238063, 10.377075030071047, 10.353919996558806, 10.330724243321061, 10.307488090530867, 10.284211857915686, 10.26089586475817, 10.237540429896702, 10.214145871725663,
10.190712508195446, 10.167240656812236, 10.14373063463754, 10.120182758287479, 10.096597343931855, 10.072974707292996, 10.049315163644378, 10.025619027809029, 10.001886614157733, 9.978118236607028,
9.954314208617, 9.930474843188891, 9.906600452862524, 9.882691349713514, 9.858747845350347, 9.83477025091125, 9.810758877060897, 9.786714033986973, 9.762636031396545, 9.738525178512301,
9.714381784068634, 9.690206156307571, 9.665998602974568, 9.64175943131417, 9.617488948065521, 9.593187459457774, 9.568855271205337, 9.544492688503038, 9.520100016021143, 9.495677557900281,
9.471225617746246, 9.446744498624703, 9.42223450305579, 9.397695933008627, 9.373129089895716, 9.348534274567282, 9.323911787305484, 9.299261927818602, 9.274584995235085, 9.249881288097566,
9.225151104356787, 9.200394741365464, 9.175612495872084, 9.150804664014636, 9.125971541314298, 9.101113422669057, 9.076230602347284, 9.051323373981258, 9.026392030560649, 9.001436864425962,
8.976458167261935, 8.951456230090905, 8.926431343266142, 8.901383796465169, 8.876313878683014, 8.8512218782255, 8.826108082702454, 8.800972779020945, 8.775816253378483, 8.7506387912562,
8.72544067741206, 8.700222195874012, 8.674983629933177, 8.649725262137022, 8.624447374282527, 8.599150247409366, 8.573834161793101, 8.548499396938364, 8.523146231572074, 8.497774943636646,
8.472385810283232, 8.446979107864982, 8.42155511193031, 8.396114097216188, 8.370656337641488, 8.34518210630031, 8.319691675455378, 8.294185316531447, 8.268663300108745, 8.243125895916455,
8.217573372826235, 8.192005998845776, 8.16642404111239, 8.140827765886666, 8.115217438546154, 8.089593323579086, 8.063955684578172, 8.038304784234423, 8.012640884331034, 7.986964245737326,
7.961275128402736, 7.9355737913508575, 7.909860492673553, 7.8841354895251134, 7.858399038116499, 7.832651393709601, 7.806892810611615, 7.7811235421694445, 7.75534384076419, 7.729553957805694,
7.70375414372716, 7.677944647979839, 7.652125719027787, 7.626297604342701, 7.60046055039881, 7.5746148026678615, 7.548760605614171, 7.522898202689747, 7.497027836329495, 7.471149747946503,
7.445264177927403, 7.419371365627796, 7.393471549367789, 7.367564966427592, 7.3416518530431905, 7.315732444402117, 7.289806974639303, 7.263875676832989, 7.237938783000769, 7.2119965240956585,
7.1860491300023, 7.160096829533227, 7.134139850425209, 7.108178419335716, 7.082212761839421, 7.056243102424832, 7.030269664490998, 7.004292670344286, 6.978312341195272, 6.952328897155711,
6.926342557235576, 6.90035353934023, 6.874362060267632, 6.848368335705682, 6.8223725802296205, 6.796375007299535, 6.7703758292579534, 6.744375257327518, 6.71837350160876, 6.692370771077954,
6.666367273585074, 6.640363215851818, 6.614358803469752, 6.5883542408985125, 6.562349731464111, 6.53634547735734, 6.510341679632234, 6.484338538204667, 6.458336251850981, 6.432335018206757,
6.406335033765636, 6.380336493878242, 6.354339592751192, 6.3283445234461935, 6.302351477879216, 6.276360646819773, 6.250372219890258, 6.2243863855654045, 6.198403331171785, 6.17242324288743,
6.146446305741532, 6.120472703614188, 6.094502619236293, 6.068536234189457, 6.042573728906033, 6.016615282669225, 5.990661073613268, 5.964711278723696, 5.938766073837687, 5.912825633644479,
5.88689013168589, 5.860959740356881, 5.835034630906218, 5.809114973437218, 5.783200936908549, 5.757292689135123, 5.731390396789048, 5.705494225400675, 5.6796043393597015, 5.653720901916358,
5.627844075182647, 5.601974020133692, 5.576110896609114, 5.550254863314512, 5.524406077822986, 5.498564696576739, 5.472730874888775, 5.446904766944602, 5.421086525804062, 5.395276303403193,
5.369474250556156, 5.343680516957251, 5.31789525118297, 5.292118600694113, 5.2663507118379975, 5.24059172985068, 5.214841798859281, 5.189101061884344, 5.163369660842257, 5.13764773654775,
5.111935428716411, 5.086232875967308, 5.060540215825608, 5.034857584725314, 5.009185118012001, 4.983522949945639, 4.957871213703449, 4.932230041382823, 4.906599564004285, 4.8809799115145065,
4.855371212789368, 4.829773595637063, 4.804187186801281, 4.778612111964378, 4.753048495750646, 4.727496461729604, 4.70195613241933, 4.676427629289851, 4.650911072766556, 4.625406582233662,
4.599914276037733, 4.574434271491199, 4.548966684875962, 4.523511631447008, 4.498069225436058, 4.472639580055292, 4.4472228075010385, 4.421819018957578, 4.396428324600928, 4.3710508336026646,
4.345686654133818, 4.320335893368744, 4.294998657489071, 4.269675051687653, 4.244365180172554, 4.219069146171082, 4.193787051933816, 4.168518998738692, 4.1432650868951075, 4.118025415748031,
4.0928000836821745, 4.067589188126153, 4.042392825556684, 4.017211091502826, 3.99204408055021, 3.9668918863453015, 3.9417546015997083, 3.9166323180944627, 3.8915251266843693, 3.8664331173023427,
3.8413563789637557, 3.816294999770859, 3.7912490669171355, 3.7662186666917528, 3.7412038844839604, 3.7162048047875604, 3.6912215112053506, 3.6662540864536073, 3.64130261236655, 3.6163671699008813,
3.5914478391402422, 3.566544699299783, 3.54165782873066, 3.5167873049245806, 3.4919332045183857, 3.467095603298567, 3.4422745762058558, 3.4174701973397923, 3.3926825399633174, 3.367911676507351,
3.343157678575384, 3.3184206169480905, 3.2937005615879276, 3.268997581643731, 3.244311745455361, 3.219643120558276, 3.194991773688189, 3.170357770785669, 3.145741177000767, 3.1211420566976553,
3.096560473459224, 3.0719964900917347, 3.0474501686294406, 3.0229215703391965, 2.998410755725102, 2.9739177845331124, 2.9494427157556693, 2.9249856076363088, 2.900546517674293, 2.8761255026291934,
2.8517226185255495, 2.8273379206574205, 2.802971463593023, 2.7786233011793158, 2.754293486546583, 2.7299820721130352, 2.7056891095893807, 2.681414649983399, 2.6571587436045014, 2.632921440068314,
2.608702788301203, 2.584502836544832, 2.560321632360699, 2.536159222634673, 2.5120156535814875, 2.487890970749291, 2.4637852190241087, 2.4396984426343575, 2.4156306851553246, 2.3915819895136394,
2.367552397991732, 2.343541952232279, 2.319550693242661, 2.2955786613993787, 2.2716258964524574, 2.247692437529878, 2.22377832314195, 2.199883591185694, 2.1760082789492348, 2.152152423116112,
2.128316059769654, 2.10449922439731, 2.0807019518949357, 2.0569242765711166, 2.0331662321514674, 2.0094278517828688, 1.9857091680377614, 1.9620102129183685, 1.9383310178609374, 1.914671613739948,
1.8910320308723119, 1.8674122990215736, 1.8438124474020494, 1.8202325046830037, 1.796672498992777, 1.7731324579229213, 1.749612408532279, 1.7261123773510985, 1.7026323903850944, 1.679172473119504,
1.655732650523131, 1.632312947052366, 1.6089133866551961, 1.5855339927751828, 1.5621747883554544, 1.5388357958426386, 1.5155170371908078, 1.4922185338654064, 1.468940306847145, 1.445682376635876,
1.4224447632544877, 1.3992274862527214, 1.3760305647110247, 1.3528540172443453, 1.3296978620059416, 1.3065621166911559, 1.2834467985411688, 1.2603519243467272, 1.2372775104518858, 1.2142235727577084,
1.1911901267259282, 1.1681771873826463, 1.1451847693219488, 1.1222128867095513, 1.099261553286409, 1.0763307823723007, 1.053420586869393, 1.030530979265821, 1.0076619716391857, 0.9848135756600946,
0.9619858025956454, 0.9391786633129122, 0.916392168282389, 0.8936263275814476, 0.8708811508977359, 0.8481566475325959, 0.825452826404435, 0.8027696960520885, 0.7801072646381755, 0.7574655399523955,
0.7348445294148576, 0.7122442400793698, 0.6896646786366745, 0.6671058514177268, 0.6445677643969088, 0.6220504231952404, 0.5995538330835721, 0.5770779989857431, 0.5546229254817518, 0.5321886168108794,
0.5097750768747922, 0.4873823092406635, 0.46501031714421615, 0.44265910349280446, 0.4203286708684324, 0.3980190215307928, 0.37573015742024296, 0.3534620801608064, 0.33121479106311114, 0.30898829112736514,
0.2867825810462481, 0.26459766120782646, 0.24243353169845072, 0.2202901923056153, 0.19816764252079544, 0.17606588154230352, 0.15398490827807798, 0.13192472134848643, 0.10988531908909671, 0.08786669955343562,
0.06586886051572982, 0.04389179947361832, 0.021935513650857885, 0.0 };

namespace voreen {
	CosmologyYearProvider::CosmologyYearProvider()
		: Processor()
		, outport_(Port::OUTPORT, "outport", "Lookback Time")
		, timeStep_("timeStep", "Time Step", 0.0f, 0.0f, 624.0f)
	{
		addPort(outport_);

		addProperty(timeStep_);

		//ON_CHANGE(timeStep_, CosmologyYearProvider, timeStepChanged);

	}

	CosmologyYearProvider::~CosmologyYearProvider() {
	}

	void CosmologyYearProvider::initialize() {
		Processor::initialize();
	}

	void CosmologyYearProvider::deinitialize() {
		Processor::deinitialize();
	}

	void CosmologyYearProvider::process() {
		int timeStep = (int)roundf(timeStep_.get());
		float lookBackTime = gyrAtStep[timeStep];

		std::string outString = "circa ";
		std::ostringstream streamNum;
		streamNum << std::fixed;
		streamNum << std::setprecision(2);

		if (lookBackTime >= 1) {
			float roundedTime = roundf(lookBackTime * 100) / 100;
			streamNum << roundedTime;
			outString += streamNum.str();
			outString += " billion years ago";
		}
		else {
			lookBackTime *= 1000;
			if (lookBackTime >= 1) {
				float roundedTime = roundf(lookBackTime * 100) / 100;
				streamNum << roundedTime;
				outString += streamNum.str();
				outString += " million years ago";
			}
			else {
				lookBackTime *= 1000;
				if (lookBackTime > 0) {
					float roundedTime = roundf(lookBackTime * 100) / 100;
					streamNum << roundedTime;
					outString += streamNum.str();
					outString += " years ago";
				}
				else {
					outString += "now";
				}
			}
		}

		outport_.setData(outString);
	}
}