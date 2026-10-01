import unittest
from minidash_live.discovery import DiscoveredEcu, DiscoverySnapshot
from minidash_live.identity import EcuIdentity, VehicleIdentity
from minidash_live.instruments import PROFILES, select_instrument_profile, encode_instrument_profile
from minidash_live.display_bridge import SerialDisplayBridge

def discovery(model='Tiguan II / MQB',year=2017,asam='EV_ECM20TFS0115NA907115D',manufacturer='Volkswagen',observed=True,errors=()):
    identity=EcuIdentity(asam_id=asam)
    engine=DiscoveredEcu('engine',identity,identity.fingerprint(),
        ({'target':'ecu.asam_id','value':asam},) if observed else (),errors)
    return DiscoverySnapshot(VehicleIdentity(manufacturer=manufacturer,model=model,model_year=year),(engine,))

class InstrumentTests(unittest.TestCase):
    def test_all_profiles_match_generation_and_observed_fuel(self):
        for p in PROFILES.values():
            with self.subTest(profile=p['id']):
                d=discovery(p['models'][0],p['years'][0],f"EV_ECM20{p['fuel']}011",p['manufacturer'])
                self.assertEqual(select_instrument_profile(d),p['id'])
                self.assertEqual(select_instrument_profile(d,market='other'),'default')

    def test_no_guess_from_unknown_model_fuel_or_seed_only(self):
        for d in [discovery('Tiguan'),discovery(year=2008),discovery(asam='EV_ECM20TDI011'),
                  discovery(observed=False),discovery(errors=({'error':'identity_conflict'},)),
                  discovery(asam='EV_ECM_UNKNOWN'),discovery('Golf VII',asam='EV_ECM20TDI011')]:
            self.assertEqual(select_instrument_profile(d),'default')

    def test_frames_allowlist_ttl_and_no_identity_secrets(self):
        self.assertEqual(encode_instrument_profile('golf-7-gtd-eu'),b'MDV1|golf-7-gtd-eu|3000\n')
        for name,ttl in [('VIN|secret',100),('unknown',100),('default',0),('default',60001),('default',True)]:
            with self.assertRaises(ValueError): encode_instrument_profile(name,ttl)

    def test_every_batch_carries_profile_before_samples(self):
        class Port:
            def __init__(self): self.frames=[]
            def write(self,frame): self.frames.append(frame);return len(frame)
            def close(self): pass
        port=Port();bridge=SerialDisplayBridge(port)
        snapshot={'decisions':{},'signals':[]}
        bridge.publish(snapshot,['engine.rpm'],instrument_profile='golf-7-gtd-eu')
        bridge._queue.join()
        bridge.publish(snapshot,['engine.rpm'],instrument_profile='golf-7-gtd-eu')
        bridge.close()
        self.assertEqual(port.frames[::2],[b'MDV1|golf-7-gtd-eu|3000\n']*2)

if __name__=='__main__': unittest.main()
