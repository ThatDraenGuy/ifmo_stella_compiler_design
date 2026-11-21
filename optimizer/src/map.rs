use std::{
    collections::VecDeque,
    marker::PhantomData,
    ops::{BitAnd, BitAndAssign, BitOr, BitOrAssign, Index, IndexMut},
};

use bitvector::BitVector;

#[allow(private_bounds)] //by design
pub trait VecKey: InnerVecKey {}
trait InnerVecKey: Copy + Eq {
    fn as_index(&self) -> usize;
    fn from_index(idx: usize) -> Self;
}
impl<K: InnerVecKey> VecKey for K {}

pub struct VecMap<K: VecKey, V> {
    inner: Vec<Option<V>>,
    phantom: PhantomData<K>,
}
impl<K: VecKey, V> VecMap<K, V> {
    pub fn new() -> Self {
        Self {
            inner: Vec::new(),
            phantom: PhantomData::default(),
        }
    }
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            inner: Vec::with_capacity(capacity),
            phantom: PhantomData::default(),
        }
    }

    pub fn insert(&mut self, value: V) -> K {
        let key = K::from_index(self.inner.len());
        self.inner.push(Some(value));
        key
    }
    pub fn get(&self, key: K) -> Option<&V> {
        self.inner.get(key.as_index()).map(Option::as_ref).flatten()
    }
    pub fn get_mut(&mut self, key: K) -> Option<&mut V> {
        self.inner
            .get_mut(key.as_index())
            .map(Option::as_mut)
            .flatten()
    }
    pub fn len(&self) -> usize {
        self.inner.len()
    }
    pub fn iter(&self) -> impl ExactSizeIterator<Item = (K, &V)> {
        self.inner
            .iter()
            .enumerate()
            .map(|(idx, v)| (K::from_index(idx), v.as_ref().unwrap()))
    }
    pub fn drain(self) -> impl ExactSizeIterator<Item = (K, V)> {
        self.inner
            .into_iter()
            .enumerate()
            .map(|(idx, v)| (K::from_index(idx), v.unwrap()))
    }

    pub fn bitmap_zeros(&self) -> BitMap<K> {
        BitMap::zeroes(self.len())
    }
    pub fn bitmap_ones(&self) -> BitMap<K> {
        BitMap::ones(self.len())
    }
    pub fn bitmaps_zeros(&self) -> ViewMap<K, BitMap<K>> {
        ViewMap {
            inner: vec![BitMap::zeroes(self.len()); self.len()],
            phantom: PhantomData::default(),
        }
    }
    pub fn bitmaps_ones(&self) -> ViewMap<K, BitMap<K>> {
        ViewMap {
            inner: vec![BitMap::ones(self.len()); self.len()],
            phantom: PhantomData::default(),
        }
    }

    pub fn empty_work_queue(&self) -> WorkQueue<K> {
        WorkQueue::empty(self.len())
    }
    pub fn work_queue(&self, order: impl Iterator<Item = K>) -> WorkQueue<K> {
        WorkQueue::new(order, self.len())
    }

    pub fn view_map<T, F: FnMut(K, &V) -> T>(&self, mut init: F) -> ViewMap<K, T> {
        ViewMap {
            inner: self.iter().map(|(k, v)| init(k, v)).collect(),
            phantom: PhantomData::default(),
        }
    }
}

impl<K: VecKey, V> Index<K> for VecMap<K, V> {
    type Output = V;

    fn index(&self, index: K) -> &Self::Output {
        self.get(index).unwrap()
    }
}
impl<K: VecKey, V> IndexMut<K> for VecMap<K, V> {
    fn index_mut(&mut self, index: K) -> &mut Self::Output {
        self.get_mut(index).unwrap()
    }
}

pub struct ViewMap<K: VecKey, V> {
    inner: Vec<V>,
    phantom: PhantomData<K>,
}
impl<K: VecKey, V> ViewMap<K, V> {
    pub fn get(&self, key: K) -> Option<&V> {
        self.inner.get(key.as_index())
    }
    pub fn get_mut(&mut self, key: K) -> Option<&mut V> {
        self.inner.get_mut(key.as_index())
    }
    pub fn iter(&self) -> impl ExactSizeIterator<Item = (K, &V)> {
        self.inner
            .iter()
            .enumerate()
            .map(|(idx, v)| (K::from_index(idx), v))
    }
    pub fn iter_mut(&mut self) -> impl ExactSizeIterator<Item = (K, &mut V)> {
        self.inner
            .iter_mut()
            .enumerate()
            .map(|(idx, v)| (K::from_index(idx), v))
    }
}
impl<K: VecKey, V> Index<K> for ViewMap<K, V> {
    type Output = V;

    fn index(&self, index: K) -> &Self::Output {
        self.get(index).unwrap()
    }
}
impl<K: VecKey, V> IndexMut<K> for ViewMap<K, V> {
    fn index_mut(&mut self, index: K) -> &mut Self::Output {
        self.get_mut(index).unwrap()
    }
}

#[derive(Clone)]
pub struct BitMap<K: VecKey> {
    inner: BitVector,
    phantom: PhantomData<K>,
}
impl<K: VecKey> BitMap<K> {
    fn zeroes(bits: usize) -> Self {
        Self {
            inner: BitVector::new(bits),
            phantom: PhantomData::default(),
        }
    }
    fn ones(bits: usize) -> Self {
        Self {
            inner: BitVector::ones(bits),
            phantom: PhantomData::default(),
        }
    }

    pub fn clear(&mut self) {
        self.inner.clear();
    }

    /// insert a new element to set
    ///
    /// If value is inserted, return true,
    /// if value already exists in set, return false.
    ///
    /// Insert, remove and contains do not do bound check.
    pub fn insert(&mut self, key: K) -> bool {
        self.inner.insert(key.as_index())
    }

    /// remove an element from set
    ///
    /// If value is removed, return true,
    /// if value doesn't exist in set, return false.
    ///
    /// Insert, remove and contains do not do bound check.
    pub fn remove(&mut self, key: K) -> bool {
        self.inner.remove(key.as_index())
    }

    /// If `bit` belongs to set, return `true`, else return `false`.
    ///
    /// Insert, remove and contains do not do bound check.
    pub fn contains(&self, key: K) -> bool {
        self.inner.contains(key.as_index())
    }
    pub fn is_empty(&self) -> bool {
        self.inner.is_empty()
    }
    pub fn len(&self) -> usize {
        self.inner.len()
    }

    pub fn iter(&self) -> impl Iterator<Item = K> + use<'_, K> {
        self.inner.iter().map(|idx| K::from_index(idx))
    }

    pub fn difference(&self, other: &Self) -> Self {
        Self {
            inner: self.inner.difference(&other.inner),
            phantom: PhantomData::default(),
        }
    }
    pub fn intersection(&self, other: &Self) -> Self {
        Self {
            inner: self.inner.intersection(&other.inner),
            phantom: PhantomData::default(),
        }
    }
    pub fn intersection_inplace(&mut self, other: &Self) -> &mut Self {
        self.inner.intersection_inplace(&other.inner);
        self
    }
    pub fn union(&self, other: &Self) -> Self {
        Self {
            inner: self.inner.union(&other.inner),
            phantom: PhantomData::default(),
        }
    }
    pub fn union_inplace(&mut self, other: &Self) -> &mut Self {
        self.inner.union_inplace(&other.inner);
        self
    }
}

impl<K: VecKey> BitOr for BitMap<K> {
    type Output = BitMap<K>;

    fn bitor(self, rhs: Self) -> Self::Output {
        self.union(&rhs)
    }
}
impl<'a, K: VecKey> BitOr for &'a BitMap<K> {
    type Output = BitMap<K>;

    fn bitor(self, rhs: Self) -> Self::Output {
        self.union(rhs)
    }
}
impl<K: VecKey> BitOrAssign for BitMap<K> {
    fn bitor_assign(&mut self, rhs: Self) {
        self.union_inplace(&rhs);
    }
}
impl<'a, K: VecKey> BitOrAssign for &'a mut BitMap<K> {
    fn bitor_assign(&mut self, rhs: Self) {
        self.union_inplace(rhs);
    }
}

impl<K: VecKey> BitAnd for BitMap<K> {
    type Output = BitMap<K>;

    fn bitand(self, rhs: Self) -> Self::Output {
        self.intersection(&rhs)
    }
}
impl<'a, K: VecKey> BitAnd for &'a BitMap<K> {
    type Output = BitMap<K>;

    fn bitand(self, rhs: Self) -> Self::Output {
        self.intersection(rhs)
    }
}
impl<K: VecKey> BitAndAssign for BitMap<K> {
    fn bitand_assign(&mut self, rhs: Self) {
        self.intersection_inplace(&rhs);
    }
}
impl<'a, K: VecKey> BitAndAssign for &'a mut BitMap<K> {
    fn bitand_assign(&mut self, rhs: Self) {
        self.intersection_inplace(rhs);
    }
}

impl<K: VecKey> PartialEq for BitMap<K> {
    fn eq(&self, other: &Self) -> bool {
        self.inner == other.inner
    }
}
impl<K: VecKey> Eq for BitMap<K> {}

pub struct WorkQueue<K: VecKey> {
    queue: VecDeque<K>,
    present: BitMap<K>,
}
impl<K: VecKey> WorkQueue<K> {
    fn empty(bits: usize) -> Self {
        Self {
            queue: VecDeque::new(),
            present: BitMap::zeroes(bits),
        }
    }
    fn new(items: impl Iterator<Item = K>, bits: usize) -> Self {
        let mut present = BitMap::zeroes(bits);
        let mut queue = VecDeque::with_capacity(bits);
        for key in items {
            present.insert(key);
            queue.push_back(key);
        }

        Self { queue, present }
    }

    pub fn push_back(&mut self, key: K) {
        if self.present.insert(key) {
            self.queue.push_back(key);
        }
    }
    pub fn pop_front(&mut self) -> Option<K> {
        let maybe_key = self.queue.pop_front();
        if let Some(key) = maybe_key {
            self.present.remove(key);
        }
        maybe_key
    }
}

mod block {
    use crate::map::InnerVecKey;

    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
    pub struct BlockId(usize);
    impl InnerVecKey for BlockId {
        #[inline]
        fn as_index(&self) -> usize {
            self.0
        }

        #[inline]
        fn from_index(idx: usize) -> Self {
            Self(idx)
        }
    }
}
pub use block::*;

mod var {
    use crate::map::InnerVecKey;

    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
    pub struct VarId(usize);
    impl InnerVecKey for VarId {
        #[inline]
        fn as_index(&self) -> usize {
            self.0
        }

        #[inline]
        fn from_index(idx: usize) -> Self {
            Self(idx)
        }
    }
}
pub use var::*;

mod instr {
    use crate::map::InnerVecKey;

    #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
    pub struct InstrId(usize);
    impl InnerVecKey for InstrId {
        #[inline]
        fn as_index(&self) -> usize {
            self.0
        }

        #[inline]
        fn from_index(idx: usize) -> Self {
            Self(idx)
        }
    }
}
pub use instr::*;
