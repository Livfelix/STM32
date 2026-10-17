import importlib.util
from pathlib import Path
import unittest


class BOM(unittest.TestCase):
    def setUp(self):
        path = Path(__file__).resolve().parents[1] / 'Projeto_Modulo_Portatil_StrainGauge/gerar_bom.py'
        spec = importlib.util.spec_from_file_location('bom', path)
        self.bom = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.bom)

    def test_mixed_topology_and_opa_reserve(self):
        self.assertTrue(hasattr(self.bom, 'build_bom'), 'generator needs import-safe BOM calculation')
        rows = {row['id']: row for row in self.bom.build_bom()}
        for key, amount in {'gauges': 9, 'ina': 6, 'opa': 4, 'ref1k': 12,
                            'ra350': 6, 'filter_r': 12, 'filter_220n': 6,
                            'filter_100n': 6, 'soic': 4}.items():
            self.assertEqual(rows[key]['necessary'], amount, key)
        self.assertEqual(rows['opa']['confirmed'], 12)
        self.assertEqual(rows['opa']['reserve'], 8)
        self.assertEqual(rows['opa']['missing'], 0)
        self.assertIn('3 bypassados', rows['ra350']['note'])
        self.assertEqual(rows['decouple_100n']['necessary'], 10)

    def test_unknown_purchase_is_not_zero_or_confirmed_stock(self):
        self.assertTrue(hasattr(self.bom, 'shortage'), 'generator needs honest pending stock calculation')
        self.assertIsNone(self.bom.shortage(12, None))
        self.assertEqual(self.bom.shortage(12, 0), 12)
        self.assertEqual(self.bom.shortage(4, 12), 0)
        self.assertEqual(self.bom.shortage(6, 2), 4)
        rows = {row['id']: row for row in self.bom.build_bom()}
        self.assertIsNone(rows['ref1k']['confirmed'])
        self.assertIsNone(rows['filter_r']['missing'])
        self.assertEqual(rows['filter_r']['declared'], 'Compra anterior declarada')

    def test_power_cable_and_retired_parts(self):
        rows = {row['id']: row for row in self.bom.build_bom()}
        self.assertNotIn('old_parts', rows)
        for key in ('rg_external', 'zener', 'or_diode'):
            self.assertEqual(rows[key]['necessary'], 0, key)
        self.assertIn('1N5819', rows['or_diode']['component'])
        self.assertEqual(rows['battery_r']['necessary'], 2)
        self.assertEqual(rows['pull']['necessary'], 3)
        self.assertIn('SW_BAT+', rows['battery_r']['note'])
        self.assertEqual(rows['connector']['necessary'], 6)
        for text in ('12 metades', 'painel', 'cabo', '0,50 mm²'):
            self.assertIn(text, rows['connector']['note'])
        self.assertEqual(rows['cable']['necessary'], 6)
        for text in ('4 vias', 'dreno', '0,50 mm²', '1 m'):
            self.assertIn(text, rows['cable']['component'])
        for text in ('3 ativas', 'quarta isolada', 'equipamento'):
            self.assertIn(text, rows['cable']['note'])
        self.assertEqual(rows['charger']['necessary'], 1)
        self.assertIn('OFF', rows['charger']['note'])
        self.assertEqual(rows['cell']['necessary'], 1)
        self.assertIn('18650 protegida + suporte', rows['cell']['component'])
        self.assertIsNone(rows['cell']['confirmed'])


if __name__ == '__main__':
    unittest.main()
